# ADR-132 HTTP Stack on ESPAsyncWebServer with an Imperative-Push to Async-Pull Bridge (ADR-123 Phase 3)

## Status

Proposed. Date: 2026-06-14.

This is the **Phase 3** networking step that ADR-123 (the 2.0.0 concurrency-model
decision) anticipated: move the web stack off the cooperative `loop()` and onto the
event-driven async server. ADR-123 deferred the concrete HTTP surface to the ADR
that lands the phase. This ADR carries that surface and **supersedes ADR-109**
(ESP32 REST response coalescing buffer), whose decision was explicitly "do NOT
migrate the sync WebServer to AsyncWebServer". That objection no longer holds once
ESP8266 is dropped (ADR-128), so the decision flips here.

Status is Proposed: it is the maintainer's checkpoint. The migration is delivered
by TASK-865.9; field validation on ESP32-S3 hardware is still open (see Risks).

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-3 HTTP stack move (sync WebServer to ESPAsyncWebServer) and the imperative-push to async-pull bridge delivered by TASK-865.9; supersede ADR-109
    changed_via: adr-kit

## Context

The 2.0.0 firmware served HTTP on the **synchronous** Arduino `WebServer`
(`OTGWWebServer = WebServer`, `platform_esp32.h`), pumped from the cooperative
`loop()` via `httpServer.handleClient()`. That model accepts and serves **one
socket per call**. A single page load opens 6+ parallel sockets (HTML + JS + CSS +
versioned assets), so one socket per loop turn stretched a page load across many
iterations. TASK-817 widened the pump to four `handleClient()` drains per loop, but
that only raised the ceiling; the underlying behaviour, observed in the field as the
**sat-slider stall** and the **XHR latency ramp** (SergeantD, `#dev-sat-mqtt`), is
intrinsic to one-socket-per-poll serving from a cooperative loop.

ADR-109 attacked a *different* symptom of the same architecture: per-`sendContent()`
cost on ESP32-S3 is ~13 ms (FreeRTOS yields to lwIP on every write), so a 300-400
chunk JSON response took ~4 s. ADR-109 introduced a static 4 KB coalescing buffer
(`sTxBuf`, `HAS_REST_TX_COALESCING`) to batch writes, and **explicitly rejected the
async migration** with the reasoning: it "requires replacing every `httpServer.send*`
call site" and "needs validation on **both ESP8266 and ESP32-S3**". The second half
of that objection is the load-bearing one, and it is now obsolete: **ADR-128 dropped
ESP8266 from 2.0.0**, making the line single-target ESP32-S3. The async stack
(`AsyncTCP` / `ESPAsyncWebServer`) is ESP32-centric (the ESP8266 `ESPAsyncTCP` fork
is poorly maintained, per ADR-123), so single-targeting ESP32-S3 is exactly what
makes the migration clean. The constraint that justified ADR-109's "do not migrate"
has been removed; this ADR records the reversal.

The migration's hardest design problem is the **impedance mismatch** between the two
server APIs. The existing REST/FSexplorer code is **imperative-push**:
`sendStartJsonMap()` then N x `sendJsonMapEntry()` then `sendEndJsonMap()`, with the
`restSend*` primitives calling `httpServer.sendContent*()` immediately. ESPAsyncWeb-
Server is **pull/callback**: a handler builds one response object and the server
flushes it asynchronously, and it **faults on a double send and hangs the socket on
no send**. Roughly 200 call sites touch `httpServer` directly (restAPI ~186,
FSexplorer ~124, jsonStuff ~48). Rewriting each into native async response building
would be a large, error-prone change; the design goal was to preserve every
`sendJsonMapEntry` body and convert mechanically.

A second mismatch: the sync `WebServer` **merges** query-string and POST-body
parameters in `arg()`/`hasArg()`; the async server **separates** them
(`getParam(name, isPost)`). Silently picking one source would break either GET or
POST call sites.

The safety of the whole approach rests on one premise: **ESPAsyncWebServer runs
every handler on a single AsyncTCP service task** (`async_tcp`), serialized, never
on `loop()` and never concurrently. The ESP32Async documentation confirms this: "a
fully asynchronous server that does not run on the loop thread", callbacks may not
call `yield`/`delay`, and "you cannot send more than one response to a single
request". That single-task serialization is what makes file-static per-request state
safe and what the send-exactly-once invariant guards against.

## Decision

Move the 2.0.0 HTTP server from the synchronous `WebServer` to **ESPAsyncWebServer**
(over **AsyncTCP**), a single `AsyncWebServer server(80)` instantiated once in
`networkStuff.ino` (ADR-044) and exposed `extern` for the WebSocket (seq10) and OTA
(seq11) phases to attach to. The per-loop `httpServer.handleClient()` drains (the
four-deep TASK-817 drain plus the two in the flash-background handlers) are deleted;
HTTP now serves on the AsyncTCP task. The `OTGWWebServer = WebServer` alias is
retired from `platform_esp32.h` (its last consumer is the sync OTA update server,
which seq11 ports).

The conversion is anchored on a new compatibility-bridge header,
**`webServerCompat.h`**, which is the reusable Phase-3 pattern. Its contract:

1. **One file-static per-request context, bound first.** `currentRequest`,
   `g_restStream`, `g_responseSent`, `g_pendingHeaders`, and `g_requestBody` are
   file-static, defined once in `networkStuff.ino`. **Every route handler MUST call
   `webBeginRequest(request)` as its first statement**; it binds `currentRequest`
   and clears all send-once/header state. This is safe (no per-request heap, no
   locks) **only because** handlers are serialized on the one AsyncTCP task.

2. **Send exactly once.** The async server faults on a double send and hangs on no
   send. Every response funnels through the bridge's write helpers (`webSend*`,
   `webSendFile`, `webRequestAuth`, and the small-JSON `restBeginStream` /
   `restFinalize`), each guarded by `g_responseSent` so the first finalize wins and
   later ones are no-ops. An early-return handler that already sent (auth failure,
   414, low-heap 500) is therefore safe.

3. **Split by hazard — read vs write.** The READ side (`argCompat`, `hasArgCompat`,
   `headerCompat`, `hostHeaderCompat`, `uriCompat`, `methodCompat`,
   `authenticateCompat`, `bodyCompat`) are stateless queries on `currentRequest`
   that keep call sites reading like the old `httpServer.arg()` API. The WRITE side
   is the send-once funnel above.

4. **Preserve merged arg semantics.** `argCompat()` / `hasArgCompat()` check POST
   body first, then query string, reproducing the sync `WebServer`'s merged
   `arg()` behaviour so the ~80 converted arg sites need no per-site GET-vs-POST
   audit beyond the mechanical swap. The raw POST body (the sync server's
   `arg("plain")`) is captured by a global `onRequestBody` hook into `g_requestBody`
   (capped at `WEB_BODY_MAX_LEN` 2560, covering the 2048-byte
   `updateAllDallasLabels` worst case) and exposed via `bodyCompat()`.

5. **Headers staged then drained.** The sync server let callers `sendHeader()`
   before `send()`; the async API attaches headers to the response object.
   `webPushHeader()` queues into `g_pendingHeaders`; the send helpers drain the
   queue onto the response right before flushing.

**Body-size routing (do not buffer large bodies on the fragmented S3 heap):**

- **Bounded JSON** (REST `/api/v2`, OTDirect overrides) writes through the
  retargeted `restSend*` layer (`jsonStuff.ino`) into a per-request
  `AsyncResponseStream` opened lazily by `restBeginStream()` and flushed once by
  `restFinalize()`. The `HAS_REST_TX_COALESCING` / `sTxBuf` write path from ADR-109
  is removed: `AsyncResponseStream` buffers internally, so the coalescing buffer is
  redundant on the async write path.
- **Static files** stream straight from flash: `request->send(LittleFS, path, mime)`
  for `serveVersionedAsset` (with `.gz` sibling + `Cache-Control`/`ETag` via
  `response->addHeader`), and the ~39 KB `index.html` (`?v=<fsHash>`) streams via a
  ported chunked `beginChunkedResponse` callback — never buffered whole.

**Routing:** the 27 `httpServer.on(...)` registrations become
`server.on(path, method, handler[, onUpload][, onBody])` plus `server.onNotFound`,
which is the `/api` catch-all dispatching to `processAPI(AsyncWebServerRequest*)`.
Handlers that previously read `httpServer` globally now take an
`AsyncWebServerRequest*` parameter (`processAPI`, `upgradepic`, `handleFileUpload`,
`reBootESP`, `resetWirelessButton`, …).

The **ADR-056** admin-security contract is preserved verbatim through the bridge:
HTTP Basic Auth via `authenticateCompat()`/`webRequestAuth()`, and the CSRF
same-origin check reading `Origin`/`Referer`/`Host` via `headerCompat()`/
`hostHeaderCompat()`. No security behaviour changes; only the accessor layer does.

## Alternatives Considered

### Alternative A: Keep ADR-109's coalescing buffer on the sync WebServer (the superseded status quo)

Retain `sTxBuf` + `HAS_REST_TX_COALESCING` and the four-deep `handleClient()` drain.
This is ADR-109's own decision. Rejected: the coalescing buffer fixed *per-flush*
latency but cannot fix **parallel-socket starvation**. One-socket-per-`handleClient()`
serving from a cooperative loop is the root of the sat-slider stall / XHR latency
ramp (TASK-817); batching writes only makes each already-serialized response
shorter, it does not let six sockets progress together. ADR-109 itself flagged this
trade-off: it hid per-flush cost rather than removing the round-trips. With ESP8266
dropped (ADR-128), the one reason ADR-109 gave to *not* migrate (dual-platform
validation cost) is gone.

### Alternative B: Native async rewrite, no compatibility bridge

Rewrite all ~200 `httpServer` call sites directly against the ESPAsyncWebServer API
(build each `AsyncWebServerResponse` by hand, no `restSend*` retarget). Rejected on
KISS / minimal-change-surface grounds: it would touch every `sendJsonMapEntry` body,
multiplying the diff and the regression surface, for no functional gain over the
bridge. The bridge preserves the proven imperative-push JSON builders unchanged and
localizes the async semantics (send-once, header staging, arg merging) to one
auditable header.

### Alternative C: Raw AsyncTCP without ESPAsyncWebServer

Drive AsyncTCP directly and implement HTTP parsing/routing ourselves. Rejected:
re-implements a mature, maintained library (ESP32Async fork) for no benefit, and
ADR-123 already selected the `AsyncTCP` / `ESPAsyncWebServer` stack for the 2.0.0
concurrency model.

### Alternative D: Do nothing (accept the stall)

Ship the sat-slider stall. Rejected: it is a field-visible regression versus 1.5.x
(< 200 ms on ESP8266) and ADR-123 already committed 2.0.0 to the async model; the
web stack is the natural Phase-3 deliverable.

## Consequences

**Benefits**

- Parallel sockets are served concurrently on the AsyncTCP task instead of one per
  loop turn: the sat-slider stall / XHR latency ramp (TASK-817) is removed at the
  root, not hidden.
- The cooperative `loop()` sheds all HTTP work: the four-deep `handleClient()` drain
  and the two flash-background drains are deleted, so OT/MQTT scheduling no longer
  competes with HTTP serving inside one loop turn.
- `AsyncResponseStream` buffers internally, retiring the ADR-109 `sTxBuf` static
  4 KB buffer and the `HAS_REST_TX_COALESCING` capability split from the write path.
- One reusable, auditable bridge (`webServerCompat.h`) localizes every async hazard
  (send-once, header staging, arg-merge, body capture); ~200 call sites convert
  mechanically with their JSON bodies unchanged.

**Trade-offs**

- **OTA is intentionally dark between this task (865.9) and seq11 (865.11).** The
  `OTGWUpdateServer` still binds the synchronous `WebServer` type
  (`OTGW-ModUpdateServer-esp32.h`), and there is no sync server left to attach to,
  so `httpUpdater.setup()` is deliberately removed in `startWiFi()`. Credentials are
  still kept current so seq11's re-wire needs no settings work. A reader who sees
  broken firmware-over-WiFi in this commit must know it is a planned, temporary seam,
  not a regression.
- New runtime dependencies: `ESPAsyncWebServer` + `AsyncTCP` (ESP32Async /
  mathieucarbou fork, "best effort" maintenance per ADR-123).
- A new discipline every future handler must follow: call `webBeginRequest()` first,
  and route every response through a send-once helper. Violating either re-opens the
  double-send fault / socket-hang the bridge exists to prevent.
- `LittleFS` reads and `feedWatchDog()` now run inside handlers on the AsyncTCP task
  (`STACK_SIZE` 16384), not on the 4 KB-ish cooperative loop. Headroom must be
  confirmed on hardware (see Risks).

**Risks and mitigations**

- *Risk*: a handler runs concurrently with another, making the file-static per-request
  context unsafe. *Mitigation*: ESP32Async docs confirm a single AsyncTCP service
  task with serialized handlers and single-response-per-request; the design depends
  on this and is documented at the top of `webServerCompat.h`. If a future library
  change introduced concurrent handlers, the bridge would need per-request heap
  state and this ADR must be revisited.
- *Risk*: a callback calls `yield()`/`delay()` (forbidden on the async task) via a
  deep helper, or a long `LittleFS` walk overflows the AsyncTCP stack / trips the
  task watchdog. *Mitigation*: field-validate a full FSexplorer walk and the largest
  responses (`sendIndex`, `apifirmwarefilelist`) on ESP32-S3 with no TWDT /
  stack-overflow (TASK-865.9 field ACs). Open until hardware-confirmed.
- *Risk*: a converted handler forgets `webBeginRequest()` or sends twice.
  *Mitigation*: the send-once helpers no-op after the first finalize; the proposed
  Enforcement gate below forbids the old `httpServer.`/`handleClient(` symbols so a
  reviewer/CI catches a regression to the sync API.
- *Risk*: an `argCompat()` value is used twice in one expression and the shared
  static buffer is clobbered. *Mitigation*: documented in the header ("copy out with
  `strlcpy` before the next call"); all current call sites obey it.

## Related Decisions

- **ADR-123 (2.0.0 Concurrency Model)**: master decision; this is its Phase 3 (web).
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the constraint change that removes ADR-109's
  "validate on both platforms" objection and makes the async migration clean.
- **ADR-109 (ESP32 REST response coalescing buffer)**: **superseded by this ADR.**
  Its "do NOT migrate to AsyncWebServer" decision and its `sTxBuf` write path are
  retired. ADR-109's body and Status are left untouched per the immutability rule.
- **ADR-129 (OT-frame queue + state mutex, Phase 1)** and **ADR-130 (PIC-UART task,
  Phase 1)** and **ADR-131 (MQTT engine on espMqttClient, Phase 2)**: sibling phases
  of the same ADR-123 rollout; this ADR is independent of them but shares the
  single-AsyncTCP-task serialization assumption with the networking phases.
- **ADR-056 (Protected admin endpoint security contract)**: preserved verbatim
  through the bridge (Basic Auth + CSRF same-origin); no behaviour change.
- **ADR-035 (RESTful API compliance)** and **ADR-050 (centralized `/api/v2` dispatch
  table)**: unchanged; `processAPI` keeps the dispatch table, now reached via
  `server.onNotFound`.
- **ADR-044 (single-point-of-instantiation for globals)**: `server` and the bridge
  context globals each have exactly one definition.
- **ADR-061 (unified platform abstraction)**: the `OTGWWebServer` type alias is
  retired here; the abstraction boundary rule is respected (no new raw
  `#ifdef ESP32` outside the allowlisted files).

## References

- Task: TASK-865.9 (feat/web: migrate HTTP server, REST API and FSexplorer to
  ESPAsyncWebServer on port 80).
- New: `src/OTGW-firmware/webServerCompat.h` — the imperative-push to async-pull
  bridge (per-request context, send-once helpers, arg/header/body compat).
- `src/OTGW-firmware/networkStuff.ino` — `AsyncWebServer server(80)` + bridge context
  definitions; OTA wiring removed across the 865.9-865.11 seam.
- `src/OTGW-firmware/networkStuff.h` — `extern AsyncWebServer server` exposure;
  `OTGWWebServer httpServer` declaration removed.
- `src/libraries/Platform/src/platform_esp32.h` — `OTGWWebServer = WebServer` alias
  retired.
- `src/OTGW-firmware/jsonStuff.ino` — `restSend*` layer retargeted onto
  `AsyncResponseStream`; `sTxBuf` / `HAS_REST_TX_COALESCING` write path removed
  (supersedes ADR-109).
- `src/OTGW-firmware/restAPI.ino` — `processAPI(AsyncWebServerRequest*)`, all
  `httpServer.*` accessors converted to `*Compat`/`web*` helpers.
- `src/OTGW-firmware/FSexplorer.ino` — `server.on(...)` / `onNotFound` / `onRequestBody`
  routing; `serveVersionedAsset` + chunked `sendIndex` ported to async responses.
- `src/OTGW-firmware/OTGW-firmware.ino` — `handleClient()` drains deleted (loop +
  flash-background handlers).
- `src/OTGW-firmware/OTGW-Core.ino`, `OTDirect.ino` — handler-signature + response
  conversions.
- ESP32Async ESPAsyncWebServer docs (concepts / configuration): single AsyncTCP
  task, no `yield`/`delay` in callbacks, single response per request.

## Enforcement

Per ADR-080 a binding pattern-level decision wants a CI gate, and ADR-123 promised
each rollout phase adds its own enforceable boundary. This is the Phase-3 boundary.
The rule maps cleanly to forbidden symbols on the firmware application files: once
the migration lands, the synchronous WebServer API and its loop pump must not
reappear in app code. Declarative, deterministic, and aligned with the task's ACs.
The maintainer ratifies (or downgrades) this block at the Proposed checkpoint.

```json
{
  "forbid_pattern": [
    {"pattern": "httpServer\\.", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "Sync WebServer retired in 2.0.0 (ADR-132). Use webServerCompat.h helpers (web*/argCompat/headerCompat) on the AsyncWebServer `server`."},
    {"pattern": "handleClient\\(", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "No handleClient() pump in 2.0.0 (ADR-132): HTTP serves on the AsyncTCP task."},
    {"pattern": "\\bOTGWWebServer\\b", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "OTGWWebServer (sync WebServer alias) retired by ADR-132; use AsyncWebServer `server`."}
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
