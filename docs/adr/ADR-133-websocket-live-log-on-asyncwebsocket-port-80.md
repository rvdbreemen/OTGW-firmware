# ADR-133 WebSocket Live-Log on AsyncWebSocket at /ws on Port 80 (ADR-123 Phase 3, supersedes ADR-005)

## Status

Proposed. Date: 2026-06-14.

This is the **WebSocket half of Phase 3** of the 2.0.0 concurrency-model rollout
(ADR-123). ADR-123 anticipated it directly: "its built-in `AsyncWebSocket`
(consolidated onto port 80, retiring the separate `WebSocketsServer` on 81)".
ADR-132 landed the HTTP/REST half of Phase 3 and exposed `extern AsyncWebServer
server;` explicitly "for the WebSocket (seq10) and OTA (seq11) phases to attach
to". This ADR carries that WebSocket surface (delivered by TASK-865.10 / seq10)
and **supersedes ADR-005** (WebSocket on a dedicated port 81 via the Links2004
`WebSocketsServer` library), whose load-bearing decision (dedicated port + separate
listener + that library) is reversed here.

Status is Proposed: it is the maintainer's checkpoint. Field validation on
ESP32-S3 hardware (live-log over `ws://<host>/ws`, reconnect across WiFi/Ethernet
transitions, Safari upload-progress per ADR-025) is still open (see Risks).

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: Document the ADR-123 Phase-3 WebSocket consolidation (retire the port-81 WebSocketsServer, attach AsyncWebSocket at /ws to the shared port-80 AsyncWebServer from ADR-132) delivered by TASK-865.10; supersede ADR-005
    changed_via: adr-kit

## Context

The 2.0.0 firmware streamed the OpenTherm live-log to the Web UI over a **second,
separate** WebSocket server: `WebSocketsServer webSocket = WebSocketsServer(81)`
(Links2004 / Markus Sattler `WebSockets @ 2.3.6`), bound to its own TCP listener on
port 81 and pumped from the cooperative loop via `webSocket.loop()` at three call
sites (`doBackgroundTasks()` and the two flash-background handlers). That design is
ADR-005's decision, made in 2019 for the ESP8266 line.

Three forces make this the right moment to retire it:

1. **ADR-123 (Phase 3) already committed to the move.** The 2.0.0 concurrency model
   replaces the cooperative loop with event-driven async networking. ADR-132 landed
   the HTTP/REST stack on `ESPAsyncWebServer` (one `AsyncWebServer server(80)`,
   declared in `webServerCompat.h`, served on the AsyncTCP task, no `handleClient()`
   pump). The WebSocket was the one networking surface still on the old cooperative
   poll, and `ESPAsyncWebServer` ships a built-in `AsyncWebSocket` precisely so a
   live-log endpoint can ride the same server and the same AsyncTCP task.

2. **Two listeners are redundant once the HTTP server is async.** With HTTP already
   on `AsyncWebServer`, keeping a second `WebSocketsServer` on port 81 means a second
   TCP listener, a second per-client buffer pool (the Links2004 library defaults to a
   15 KiB per-client buffer, `WEBSOCKETS_MAX_DATA_SIZE`, unshrinkable without
   patching the library), and a per-loop `webSocket.loop()` poll that the async model
   exists to eliminate. `AsyncWebSocket` is **not a server**: it is a handler
   attached to the existing server (`server.addHandler(&otLogWs)`), so it adds an
   endpoint, not a listener. Two listeners cannot bind port 80, which is exactly why
   this task hard-depends on seq9 (ADR-132) already owning `AsyncWebServer server`.

3. **The library API mismatch must be bridged once.** The Links2004 callback is
   indexed by an opaque client number (`webSocketEvent(uint8_t num, WStype_t, ...)`)
   and the firmware kept its **own** `wsClientCount` counter in parallel.
   `AsyncWebSocket` dispatches `(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType,
   ...)` and owns its client list (`otLogWs.count()`), so the parallel counter is
   removed and every count read routes through the library. The cap test changes with
   it: `AsyncWebSocket` inserts the new client into its list **before** firing
   `WS_EVT_CONNECT` (`_clients.emplace_back()` precedes the dispatch), so the limit
   test is `count() > MAX_WEBSOCKET_CLIENTS` (count already includes the arrival),
   not the old `>=` against a pre-increment counter.

A second, browser-facing contract changes with the port: the Web UI connected to
`ws://<host>:81/`. The endpoint is now `ws://<host>/ws` on the page's own port
(80), so `data/index.js` swaps the hardcoded `WEBSOCKET_PORT = 81` for a path
`WEBSOCKET_PATH = '/ws'` and builds the URL from `window.location.host`. The
reconnect/watchdog/keepalive lifecycle and the keepalive JSON shape are unchanged,
so no Web UI behaviour changes beyond the URL.

Two invariants from earlier ADRs are **preserved verbatim** and are load-bearing:

- **ADR-121 (per-consumer heap gate).** `sendLogToWebSocket()` keeps its
  `&& canSendWebSocket()` guard in the OT-hot path. This is the parity floor that
  fixed George's "MQTT goes unavailable with the live-log open" report; removing it
  would regress that fix, so it stays on the new `otLogWs.textAll()` path.
- **ADR-025 (Safari WebSocket connection management).** The 30 s application-level
  keepalive and the browser-side `flashModeActive` upload guard are retained; the
  keepalive is now emitted from a timer (the loop no longer polls the library) but
  its JSON shape is identical.

## Decision

Retire the dedicated port-81 `WebSocketsServer` and stream the OpenTherm live-log
over a single **`AsyncWebSocket otLogWs("/ws")`** attached to the shared port-80
`AsyncWebServer server` (ADR-132) via `server.addHandler(&otLogWs)`. The endpoint is
`ws://<host>/ws`; there is no dedicated WebSocket port and no per-loop poll. The
Links2004 `WebSockets @ 2.3.6` dependency is dropped from `platformio.ini`; the
`AsyncWebSocket` type comes from the `ESPAsyncWebServer` dependency that ADR-132 /
seq4 already own.

The decision has five concrete parts:

1. **Attach-once, idempotent `startWebSocket()`.** `startWebSocket()` is called from
   `setup()` and again on **every** WiFi and Ethernet transition (three call sites:
   `OTGW-firmware.ino`, `networkStuff.ino`, `Ethernet.ino`). Those transitions do
   **not** tear down the port-80 server, so the handler must be attached exactly
   once. `startWebSocket()` guards on the existing `wsInitialized` flag and returns
   early if already attached; `otLogWs.onEvent()` + `server.addHandler()` run once.
   Re-binding on a transition would be the bug; the no-op is correct. `addHandler()`
   after `server.begin()` is safe because `begin()` only starts the TCP listener and
   the handler list is consulted live per request.

2. **Library-owned client accounting.** The parallel `wsClientCount` is deleted.
   `hasWebSocketClients()`, the burst-window snapshot, and the send guards all read
   `otLogWs.count()`. The connect-time cap is `count() > MAX_WEBSOCKET_CLIENTS`
   (cap = 3, unchanged), enforced by `client->close()` on the over-limit arrival,
   and the heap-health reject (`platformFreeHeap() < HEAP_WARNING_THRESHOLD`) and the
   burst-window diagnostics (`noteWebSocketBurstEvent`) are preserved.

3. **Event-handler port to the async signature.** `webSocketEvent` is rewritten to
   the `AwsEventHandler` signature and the `WStype_*` switch becomes
   `WS_EVT_CONNECT` / `WS_EVT_DISCONNECT` / `WS_EVT_DATA` / `WS_EVT_ERROR` /
   `WS_EVT_PING` / `WS_EVT_PONG`, using `client->id()` / `client->remoteIP()` /
   `client->close()` in place of the opaque `num` index and `webSocket.*(num)` calls.

4. **All transmit paths route through `otLogWs.textAll()`.** `sendLogToWebSocket()`
   (OT-hot path, **keeps** `&& canSendWebSocket()` per ADR-121), `sendWebSocketJSON()`
   (firmware-upgrade progress, ~12 call sites across OTDirect, OTGW-Core, SATcontrol,
   SATcycles), the 30 s keepalive, `doWebSocketClose()` and `doWebSocketDisconnectAll()`
   (→ `otLogWs.closeAll()`) all use the `AsyncWebSocket` broadcast API. No
   `broadcastTXT` / `disconnect(num)` remain in firmware.

5. **`handleWebSocket()` becomes timer-driven housekeeping.** The `webSocket.loop()`
   poll is removed (the socket is serviced on the AsyncTCP task). `handleWebSocket()`
   now only does what the library cannot do on its own: `otLogWs.cleanupClients(MAX_WEBSOCKET_CLIENTS)`
   to reap stale slots, plus the 30 s ADR-025 keepalive. It is called from a 1 s
   `DECLARE_TIMER_SEC` in `doBackgroundTasks()` rather than every loop turn, and the
   two flash-background `handleWebSocket()` calls are removed (fwupgradestep progress
   keeps the socket warm during flash; the AsyncTCP task serves it regardless).

The **ADR-005 trust model is preserved**: the endpoint is unauthenticated and for
trusted-LAN use only (ADR-003 ws://, not wss://). Only the transport mechanism
(dedicated port 81 + separate `WebSocketsServer` → `/ws` path on the shared port-80
`AsyncWebServer`) changes.

## Alternatives Considered

### Alternative A: Keep the dedicated port-81 WebSocketsServer (ADR-005, the superseded status quo)

Leave `WebSocketsServer(81)` and the `webSocket.loop()` poll in place even after the
HTTP server went async. **Rejected.** It keeps a second TCP listener and a second
15 KiB-per-client buffer pool alongside the async server for no benefit, and it keeps
the cooperative `webSocket.loop()` poll that ADR-123 Phase 3 exists to remove. With
HTTP already on `AsyncWebServer` (ADR-132) the marginal cost of consolidating the
WebSocket onto the same server is small and the redundancy cost of *not* doing so is
permanent. ADR-005's port-81 decision predates the async model by seven years and the
ESP8266 platform it was written for is dropped from 2.0.0 (ADR-128).

### Alternative B: A second AsyncWebSocket-style server on a separate port

Stand up the new `AsyncWebSocket` but keep it on its own dedicated port (mirroring
the old port-81 split, just on the maintained library). **Rejected.** `AsyncWebSocket`
is a *handler*, not a server; isolating it would mean a second `AsyncWebServer`
instance and a second listener purely to preserve a port split that bought nothing
except a firewall line item. Consolidating onto port 80 removes the second listener,
the second port in the firewall/docs, and the `WEBSOCKET_PORT` constant in the UI.
There is no isolation benefit: HTTP and the live-log already share heap and the
AsyncTCP task; a separate port does not change that.

### Alternative C: Server-Sent Events (SSE) over the port-80 server

Drop WebSocket entirely and push the live-log as an SSE stream (`AsyncEventSource`,
also built into `ESPAsyncWebServer`). **Rejected.** SSE is one-way only, and while the
live-log is one-way today, ADR-005 deliberately kept the bidirectional WebSocket for a
possible future command channel from the UI; throwing that away is a larger,
behaviour-changing decision than this consolidation. It would also force a Web UI
rewrite (EventSource API, different reconnect semantics) for no transport-cost win
over an `AsyncWebSocket` that already rides the same server. The minimal-change path
keeps the WebSocket contract and only moves its address.

### Alternative D: Do nothing this phase (defer the WebSocket move)

Ship Phase 3 with HTTP async but the WebSocket still on port 81. **Rejected.** It
leaves the one remaining `webSocket.loop()` poll in the loop, so the cooperative-poll
removal ADR-123 Phase 3 promised would be incomplete, and it strands the Links2004
dependency and its 15 KiB-per-client buffers in the build. ADR-132 explicitly scoped
the WebSocket as seq10 and exposed `extern AsyncWebServer server` for it; finishing
the phase now is the planned, low-risk completion.

## Consequences

**Benefits**

- One TCP listener instead of two: the port-81 listener and the entire Links2004
  `WebSockets @ 2.3.6` dependency (with its 15 KiB-per-client buffer pool) are gone;
  the live-log shares the port-80 `AsyncWebServer` and its AsyncTCP task.
- The last cooperative `webSocket.loop()` poll leaves the loop; `handleWebSocket()`
  drops from three per-loop call sites to one 1 s-timer housekeeping call, and the
  two flash-background pumps are removed. The socket is serviced on the AsyncTCP task
  regardless of loop load.
- The Web UI loses a hardcoded port: `ws://<host>:81/` → `ws://<host>/ws` derived
  from `window.location.host`, so the live-log works through any reverse proxy that
  already fronts the port-80 UI, and the firewall/docs lose the port-81 line.
- Client accounting has one source of truth (`otLogWs.count()`); the bug surface of a
  hand-maintained `wsClientCount` drifting from the real client list is eliminated.

**Trade-offs**

- New behaviour every WebSocket touch must respect: `startWebSocket()` is now
  attach-once (idempotent), so the per-transition re-bind that was harmless with a
  self-contained `WebSocketsServer` would now be a double-`addHandler` bug. The
  `wsInitialized` early-return is the guard.
- The connect-cap test inverted from `>=` to `> MAX` because `AsyncWebSocket` inserts
  the client before dispatching `WS_EVT_CONNECT`. An off-by-one here would silently
  admit a 4th client or reject the 3rd; the rationale is documented at the handler.
- The dependency surface shifts from a self-contained WebSocket library to the
  `ESPAsyncWebServer` `AsyncWebSocket` (ESP32Async / mathieucarbou fork, "best
  effort" maintenance per ADR-123). The live-log now shares that fork's fate with the
  HTTP stack rather than being independently sourced.

**Risks and mitigations**

- *Risk*: a WiFi/Ethernet transition re-enters `startWebSocket()` and re-attaches the
  handler, corrupting the server's handler list. *Mitigation*: the `wsInitialized`
  early-return makes `startWebSocket()` idempotent; the three call sites are
  documented at the function. Field-validate a WiFi↔Ethernet failover with a live-log
  tab open and confirm the stream resumes without a duplicate handler (TASK-865.10
  field ACs, open until hardware-confirmed).
- *Risk*: the `count() > MAX` cap admits one too many or rejects one too few because a
  future library version changes when the client is inserted relative to the event.
  *Mitigation*: the insertion-order assumption is documented at the handler with the
  library source reference; the burst-window diagnostics surface the live count so a
  drift is observable in the field.
- *Risk*: removing the OT-hot-path heap gate while retargeting to `otLogWs.textAll()`
  would regress the ADR-121 fix (MQTT unavailable with the live-log open).
  *Mitigation*: `sendLogToWebSocket()` keeps `&& canSendWebSocket()` verbatim; the
  evaluator AC greps for its retention and for the absence of `broadcastTXT` / port
  81 / `WEBSOCKET_PORT`.
- *Risk*: dropping the per-loop poll breaks Safari's keepalive expectation or the idle
  watchdog (ADR-025). *Mitigation*: the 30 s keepalive is preserved (now timer-driven,
  identical JSON shape) and `cleanupClients()` reaps stale slots; field-validate
  Safari upload-progress on macOS + iOS and an extended idle live-log session
  (TASK-865.10 field ACs, open until hardware-confirmed).

## Related Decisions

- **ADR-005 (WebSocket for Real-Time Streaming)**: **superseded by this ADR.** Its
  decision (dedicated port 81 + separate `WebSocketsServer` from Links2004) is
  reversed; the WebSocket-streaming *concept* it established survives, re-homed onto
  `AsyncWebSocket` at `/ws` on port 80. ADR-005's body and Status are left untouched
  per the immutability rule.
- **ADR-123 (2.0.0 Concurrency Model)**: master decision; this is the WebSocket half
  of its Phase 3 (it named "AsyncWebSocket consolidated onto port 80, retiring the
  separate WebSocketsServer on 81").
- **ADR-132 (HTTP stack on ESPAsyncWebServer, Phase 3 web)**: hard dependency. It
  instantiates the single `AsyncWebServer server(80)` and exposes it `extern` (via
  `webServerCompat.h`) "for the WebSocket (seq10)" to attach to; this ADR does
  `server.addHandler(&otLogWs)`.
- **ADR-128 (Drop ESP8266 from 2.0.0)**: the constraint change that makes the
  ESP32-centric `AsyncWebSocket` the single code path; ADR-005's ESP8266-era port-81
  design no longer has a platform to serve.
- **ADR-121 (per-consumer heap gating, WebSocket vs MQTT)**: **preserved.** The
  `&& canSendWebSocket()` OT-hot-path gate is carried verbatim onto the new transmit
  path; this ADR does not alter the heap-gate contract.
- **ADR-025 (Safari WebSocket connection management)**: **preserved.** The 30 s
  keepalive (now timer-driven, identical JSON) and the browser-side `flashModeActive`
  upload guard are retained.
- **ADR-107 (emergency heap recovery actions)**: `doWebSocketDisconnectAll()` (action
  #1) is retargeted to `otLogWs.closeAll()`; behaviour unchanged.
- **ADR-056 (protected admin endpoint security contract)**: unaffected; the live-log
  endpoint is unauthenticated trusted-LAN-only as before (ADR-003 ws://), and the
  admin-security bridge lives in the HTTP path (ADR-132).
- **ADR-010 (multiple concurrent network services / port allocation)**: the port-81
  allocation it recorded is retired by this consolidation; ADR-010's broader
  re-scoping is owned by ADR-123 Phase 4, not this ADR.

## References

- Task: TASK-865.10 (feat/web: migrate the WebSocket live-log from `WebSocketsServer`
  on 81 to `AsyncWebSocket` on port 80), seq10 of the ADR-123 rollout.
- `src/OTGW-firmware/webSocketStuff.ino`: `AsyncWebSocket otLogWs("/ws")`; idempotent
  `startWebSocket()` (`server.addHandler`), `webSocketEvent` async signature,
  `otLogWs.textAll()` / `closeAll()` / `count()` / `cleanupClients()`; `wsClientCount`
  removed; `webSocket.loop()` removed.
- `src/OTGW-firmware/networkStuff.h`: `#include <WebSocketsServer.h>` removed; endpoint
  doc updated to `ws://<host>/ws`.
- `src/OTGW-firmware/OTGW-firmware.ino`: `handleWebSocket()` moved behind a 1 s
  `DECLARE_TIMER_SEC` in `doBackgroundTasks()`; the two flash-background
  `handleWebSocket()` pumps removed.
- `src/OTGW-firmware/OTGW-firmware.h`: `doWebSocketClose()` comment updated
  (`otLogWs` not extern'd in any header).
- `src/OTGW-firmware/data/index.js`: `WEBSOCKET_PORT = 81` → `WEBSOCKET_PATH = '/ws'`;
  `ws://<host>:81/` → `ws://<host>/ws` from `window.location.host`; ADR-025
  `flashModeActive` guard retained.
- `platformio.ini`: `WebSockets @ 2.3.6` (Links2004 / Markus Sattler) dependency
  removed; `AsyncWebSocket` comes from the `ESPAsyncWebServer` dependency (ADR-132 / seq4).
- `src/OTGW-firmware/webServerCompat.h`: `extern AsyncWebServer server;` (the seq9
  interface this attaches to).
- ESP32Async ESPAsyncWebServer: `AsyncWebSocket` is a handler attached via
  `server.addHandler(&ws)`, serviced on the AsyncTCP task; `cleanupClients()`,
  `count()`, `textAll()`, `closeAll()` API: <https://github.com/mathieucarbou/ESPAsyncWebServer>

## Enforcement

Per ADR-080 a binding pattern-level decision wants a CI gate, and ADR-123 promised
each rollout phase adds its own enforceable boundary. This is the WebSocket boundary
of Phase 3. The rule maps cleanly to forbidden symbols on the firmware application
files and the Web UI asset: once the migration lands, the Links2004 `WebSocketsServer`
API, its loop poll, its broadcast call, and the port-81 URL must not reappear. This
mirrors the evaluator AC ("grep zero `WebSocketsServer` / `webSocket.loop()` /
`broadcastTXT` / port 81 in firmware + `WEBSOCKET_PORT` / `:81` in index.js").
Declarative and deterministic; the maintainer ratifies (or downgrades) this block at
the Proposed checkpoint.

```json
{
  "forbid_pattern": [
    {"pattern": "WebSocketsServer", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "Links2004 WebSocketsServer retired in 2.0.0 (ADR-133). Use AsyncWebSocket otLogWs on the shared AsyncWebServer `server` (server.addHandler)."},
    {"pattern": "webSocket\\.loop\\(", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "No webSocket.loop() poll in 2.0.0 (ADR-133): the live-log is serviced on the AsyncTCP task; only cleanupClients()/keepalive run from a timer."},
    {"pattern": "broadcastTXT", "path_glob": "src/OTGW-firmware/*.{ino,h}",
     "message": "broadcastTXT is the Links2004 API (ADR-133). Use otLogWs.textAll() on the AsyncWebSocket."},
    {"pattern": "WEBSOCKET_PORT", "path_glob": "src/OTGW-firmware/data/index.js",
     "message": "No dedicated WebSocket port in 2.0.0 (ADR-133): connect to ws://<host>/ws via WEBSOCKET_PATH on window.location.host."}
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false
}
```
