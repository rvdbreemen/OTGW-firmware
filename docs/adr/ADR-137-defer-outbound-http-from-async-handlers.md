# ADR-137 Defer Outbound HTTP Initiated From an Async Handler to loop() Context (Amends ADR-132)

## Status

Proposed, 2026-06-14.

This ADR amends ADR-132 (HTTP stack on ESPAsyncWebServer with an imperative-push to
async-pull bridge). ADR-132 stays in force and Accepted; this ADR generalizes one of
its rules. Per the immutability convention, the amendment lives in this new document
rather than editing the Accepted ADR-132 body. ADR-132's "Amended by" back-reference,
if any, is the maintainer's to add at the acceptance checkpoint; this ADR expresses
the relationship from its own side (title and Related Decisions). The same pattern
this project already uses for ADR-135 (amends ADR-011) and ADR-089 (amends ADR-030).

Delivered by TASK-865.14. Field validation on ESP32-S3 hardware is open (see Risks)
and tracked under epic TASK-865.

## Status History

status_history:
  - date: 2026-06-14
    status: Proposed
    changed_by: Agent
    reason: "Generalize ADR-132's no-blocking-on-the-AsyncTCP-task rule to cover OUTBOUND HTTP initiated from an async handler. After the TASK-865.9 async web migration, two PIC seams (update-check HEAD, refresh GET+writeToStream) still ran blocking HTTPClient calls on the AsyncTCP task, freezing the whole web stack for the request duration. Defer both to loop() context via the existing handlePendingUpgrade() bridge. Delivered by TASK-865.14; amends ADR-132, cross-refs ADR-134, depends on ADR-135."
    changed_via: adr-kit

## Context

ADR-132 moved the 2.0.0 HTTP stack onto ESPAsyncWebServer. Its load-bearing premise:
**every handler runs on the single AsyncTCP service task** (`async_tcp`), serialized,
never on `loop()`, and a callback may not `yield()`/`delay()`. ADR-132 framed the
resulting "do not block the async task" rule around **response generation** (send
exactly once, no blocking while building the response). That framing left an implicit
gap: it did not state that a handler must not make a blocking **outbound** request
either.

Two PIC firmware-management seams fell into that gap after the 865.9 migration. Both
make synchronous `HTTPClient` calls to `http://otgw.tclcode.com` from inside
async-reachable handlers, i.e. on the AsyncTCP task:

- `GET /api/v2/pic/update-check` → `processAPI` → `handlePic` → `sendPICUpdateCheck()`
  → `checkforupdatepic()` does a blocking `http.sendRequest("HEAD")`
  (`OTGW-Core.ino:5476-5500`). It fires **automatically** when the user opens the PIC
  firmware tab (`index.js:4804`), so a user just clicking the tab triggers it.
- `GET /pic?action=refresh` → `upgradepic()` → `refreshpic()` does a blocking
  `http.GET()` + `writeToStream()` of a full Intel-HEX file to LittleFS
  (`OTGW-Core.ino:5502-5545`).

The AsyncTCP task services **all** HTTP plus the ADR-133 progress WebSocket and shares
lwIP. A multi-second outbound request, or the full DNS/connect timeout when the host is
unreachable, freezes the entire web stack for its duration. This is **worse** than the
pre-865.9 synchronous `WebServer`: there the same blocking call ran in the loop-side
`handleClient()` pump and stalled only one cooperative loop turn, not the network
service task. So the async migration, intended to remove the sat-slider stall, quietly
re-introduced a coarser freeze on exactly the auto-firing tab-open path.

This is a regression introduced by ADR-132's migration, recognizable from any future
diff that calls `http.GET`/`http.sendRequest`/`http.writeToStream` (or another blocking
network primitive) on a call path reachable from an `AsyncWebServerRequest*` handler.
It is the outbound-direction twin of the inbound-direction rule ADR-132 already states.

## Decision

ADR-132's "no blocking work on the AsyncTCP task" contract extends to **outbound HTTP**.
A handler reachable from `AsyncWebServerRequest*` (including the `processAPI` dispatch
path) MUST NOT make a blocking outbound network call. Such work is **deferred to loop()
context** using the same imperative-push to async-pull bridge the `action=upgrade` flash
already uses (the `handlePendingUpgrade()` pattern; ADR-134 is the OTA analog). The async
handler validates and QUEUES the request and returns immediately; a loop()-context worker
performs the `HTTPClient` work; the result is surfaced through cached state the existing
REST poll reads.

Concretely, for the two PIC seams (`OTGW-Core.ino`, `restAPI.ino`, `PICtypes.h`,
`OTGW-firmware.{h,ino}`, `data/index.js`):

1. **Validate + queue, return immediately.** `sendPICUpdateCheck()` becomes a pure
   status query: it kicks a check only from the `PIC_UPDATE_IDLE` state via
   `queuePicUpdateCheck()` and returns the cached result plus a `status` field
   (`"checking" | "ready" | "error" | "unavailable"`). `upgradepic()`'s `refresh` branch
   calls `queuePicRefresh(filename, version)` and returns `{"status":"started"}`, or
   `409 {"status":"busy"}` when a job is already pending.

2. **One single-flight slot.** `pendingPicHttpJob` (a `PicHttpJob` enum: `None`,
   `UpdateCheck`, `Refresh`) holds at most one job. The queue functions return `false`
   (changing nothing) if a job is already pending, so the slot is never silently
   overwritten and the two outbound calls never overlap.

3. **A loop()-context worker.** `handlePendingPicHttp()` runs in `loop()` next to
   `handlePendingUpgrade()` (both under `#if HAS_PIC`). It snapshots and clears the slot
   first (so a re-entrant `doBackgroundTasks()` from `doAutoConfigure()`'s file-reading
   loop cannot re-dispatch the same job), then performs the `HTTPClient` work off the
   AsyncTCP task. Results land in cached `state.pic` fields (`sLatestFw[32]`,
   `iUpdateCheck`) that the REST poll reads. These are written by the loop worker and
   read by the AsyncTCP handler with no barrier: an intentionally eventual-consistent
   single-slot bridge, identical to the accepted `handlePendingUpgrade()` shape, where a
   torn read costs one extra "checking" poll and self-heals on the next.

4. **Bounded timeouts under the TWDT.** The deferred call now runs on the loop task,
   which is subscribed to the 30 s ESP32 TWDT (ADR-135), and `feedWatchDog()` brackets
   the blocking calls. The **auto-firing** update-check (the regression path, fired on
   tab open) is a single `sendRequest("HEAD")` with a 5 s connect+read cap
   (`PIC_HTTP_TIMEOUT_MS`): hard-bounded at ~5 s of one loop turn, well under the
   watchdog window, so an unreachable host neither resets the device nor freezes the web
   stack. The **user-initiated** refresh download (`http.GET()` + `writeToStream()`) uses
   the 5 s connect cap and a larger 15 s per-read cap (`PIC_HTTP_DOWNLOAD_TIMEOUT_MS`,
   sized so a full ~30-60 KB hex is not truncated on a slow link, since a truncated file
   fails `validateIntelHex` and is discarded).

5. **Frontend polls instead of blocking.** `pollPICUpdateCheck()` re-polls
   `update-check` (the first call adds `?recheck=1`, which resets the server cache to
   IDLE to force exactly one fresh outbound check, preserving the original "check on
   every tab open" behaviour; re-polls are pure status reads) while `status=="checking"`,
   then renders ready/error. `pollPICRefresh()` re-polls `firmware/files` until the
   on-disk version changes after a refresh. Because the outbound HTTP never runs on the
   network task, the ADR-133 live-log WebSocket keeps flowing throughout.

This generalizes ADR-132's rule rather than reversing it: **no blocking work on the
AsyncTCP task, inbound response generation or outbound HTTP alike.** No firmware
`httpServer.*`/`handleClient(` symbol is reintroduced; ADR-132's existing Enforcement
gate is unchanged and untouched.

## Alternatives Considered

### Alternative A: Defer to loop() via the existing handlePendingUpgrade() bridge (chosen)

Queue from the async handler, run the `HTTPClient` work in `loop()`, surface the result
through cached `state.pic` state that the REST poll reads. Chosen for KISS and parity:
it reuses the exact imperative-push to async-pull bridge already accepted for the
`action=upgrade` flash (ADR-134), so the codebase gains no new concurrency primitive,
no new task, and no new lifecycle to reason about. The loop task is already subscribed
to the TWDT (ADR-135), so the bounded outbound timeouts have a watchdog backstop for
free. The cost is added latency (the result is not ready when the handler returns) and a
frontend that must poll, both of which are acceptable for a manual, user-initiated PIC
maintenance flow and mirror what `action=upgrade` already does.

### Alternative B: A transient worker FreeRTOS task per outbound request

Spawn (or wake) a dedicated FreeRTOS task that may block, run the `HTTPClient` call
there, and signal completion back to the handler/state. This is the shape ADR-136 chose
for the webhook sender (a blocking `HTTPClient` on its own `webhook` task). Rejected for
this seam on KISS / minimal-change-surface grounds: the PIC update-check and refresh are
**rare, manual, strictly one-at-a-time** operations, so they do not need a task's
concurrency. A task adds stack budget on the already-fragmented S3 heap, a second
watchdog-subscription decision, and task-lifecycle code, for no benefit over the
single-slot loop defer that already exists and is accepted. ADR-136's webhook task earns
its complexity because webhooks fire from OT-bus edges at unpredictable times and need
retry/backoff while the loop keeps running; a tab-open update-check does not.

### Alternative C: Do nothing — keep the blocking call on the AsyncTCP task

Ship the migration as-is. Rejected: it is a field-visible regression. The update-check
fires **automatically** on PIC-tab open, and an unreachable `otgw.tclcode.com` (DNS or
connect failure) freezes all HTTP and the ADR-133 live-log WebSocket for the full
default timeout. That is strictly worse than the pre-865.9 synchronous server, where the
same call stalled only one cooperative loop turn. The whole point of ADR-132 was to
remove web-stack stalls; leaving this one in place contradicts it.

## Consequences

**Benefits**

- The auto-firing PIC update-check and the user-initiated refresh download no longer run
  on the AsyncTCP task, so opening the PIC tab (or hitting an unreachable update host)
  no longer freezes other HTTP traffic or the ADR-133 live-log WebSocket.
- The unreachable-host worst case is bounded to ~5 s on one loop turn (the `HEAD`
  connect+read cap), well under the 30 s TWDT window (ADR-135), instead of the full
  default `HTTPClient` stack timeout on the network task.
- No new concurrency primitive: the fix reuses the accepted `handlePendingUpgrade()`
  single-slot bridge, so the mental model and the failure modes are already understood.

**Trade-offs**

- The update-check result is no longer ready when the handler returns; the frontend must
  poll (`pollPICUpdateCheck`/`pollPICRefresh`) until the loop worker finishes. This adds
  a few seconds of latency to a manual maintenance flow and a small amount of JS polling
  logic.
- The cached `state.pic.sLatestFw`/`iUpdateCheck` fields are written loop-side and read
  on the AsyncTCP task with no memory barrier (eventual-consistent by design). A torn
  read costs one extra "checking" poll; it cannot corrupt because the only multi-byte
  field (`sLatestFw[32]`) is consumed only when `iUpdateCheck == READY`, and a stale
  READ/CHECKING flip self-heals on the next poll.
- The single-flight slot means a second update-check or refresh while one is pending is
  rejected (`409 busy` / `queue*` returns false), not queued. For a manual one-at-a-time
  flow this is the desired behaviour, not a limitation.

**Risks and mitigations**

- *Risk*: the refresh download's `writeToStream()` runs between two `feedWatchDog()`
  calls with no intra-call feed, so a pathologically slow-but-steady stream (each read
  gap < 15 s but cumulative transfer > 30 s) could approach the TWDT window.
  *Mitigation*: bounded in practice by the small (~30-60 KB) hex size and the per-read
  15 s cap; this is the same exposure the pre-865.9 synchronous `writeToStream` already
  carried in the loop, so it is not a new regression. If a future hex grows large enough
  to matter, chunk the write with an intra-loop feed.
- *Risk*: a future handler adds a new blocking outbound call (a different host, a new
  PIC command, an OTDirect cloud call) directly on the async path, re-opening the freeze.
  *Mitigation*: the Enforcement block below flags blocking-network primitives on
  async-reachable paths for review; the rule is also stated in this ADR's Decision and
  cross-referenced from `handlePendingPicHttp()`.
- *Risk*: field behaviour on ESP32-S3 is unverified (AC#5). *Mitigation*: open
  field-validation AC under epic TASK-865 — open the PIC tab while the live-log WS
  streams and confirm it keeps flowing; refresh-download a hex with the UI responsive;
  point update-check at an unreachable host and confirm no freeze and no reset.

## Related Decisions

- **ADR-132 (HTTP stack on ESPAsyncWebServer with the imperative-push to async-pull
  bridge)**: **amended by this ADR.** ADR-132 established the single-AsyncTCP-task
  premise and the "do not block the async task" rule for response generation; this ADR
  extends that same rule to outbound HTTP. ADR-132's body, Status, and Enforcement gate
  are left untouched per the immutability rule.
- **ADR-134 (Async OTA upload handler)**: the pattern this mirrors. ADR-134's
  `action=upgrade` flash deferral (`handlePendingUpgrade()`, deferred-reboot-from-loop)
  is the imperative-push to async-pull bridge `handlePendingPicHttp()` reuses for the
  deferred PIC outbound HTTP.
- **ADR-135 (ESP32 TWDT is the primary watchdog)**: depended on. The deferred outbound
  HTTP runs on the TWDT-subscribed loop task, so the bounded `HTTPClient` timeouts
  (`PIC_HTTP_TIMEOUT_MS` = 5 s, `PIC_HTTP_DOWNLOAD_TIMEOUT_MS` = 15 s) stay under the
  30 s TWDT panic window.
- **ADR-133 (WebSocket live-log on AsyncWebSocket, port 80)**: the progress WebSocket
  that shares the AsyncTCP task. Keeping outbound HTTP off that task is what lets the
  live-log keep streaming while a PIC check/refresh runs.
- **ADR-136 (retire/simplify the WiFi/webhook/PIC state machines)**: the contrast case.
  ADR-136 chose a dedicated FreeRTOS task for the webhook's blocking `HTTPClient` because
  webhooks fire unpredictably and need concurrent retry/backoff; this ADR chose the
  loop-defer because the PIC seams are rare, manual, and one-at-a-time (Alternative B).
- **ADR-004 (no String in hot paths)**: the new state cache uses fixed `char[]`
  (`sLatestFw[32]`) and small-int enums, keeping `PICSection` POD.

## References

- Task: TASK-865.14 (defer the PIC update-check and refresh outbound HTTP off the
  AsyncTCP task; amend ADR-132).
- `src/OTGW-firmware/OTGW-Core.ino` — `PIC_HTTP_TIMEOUT_MS`/`PIC_HTTP_DOWNLOAD_TIMEOUT_MS`
  bounds and `feedWatchDog()` bracketing in `checkforupdatepic()` (HEAD) and
  `refreshpic()` (GET+writeToStream); the deferral worker `handlePendingPicHttp()` and
  the `queuePicUpdateCheck()`/`queuePicRefresh()` single-flight queue next to
  `handlePendingUpgrade()`; `upgradepic()`'s `refresh` branch now queues + returns.
- `src/OTGW-firmware/restAPI.ino` — `sendPICUpdateCheck()` converted to a status query
  (`?recheck=1` cache reset, `status` field, no inline outbound HTTP).
- `src/OTGW-firmware/PICtypes.h` — `PicUpdateCheck` enum and the `sLatestFw`/`iUpdateCheck`
  cache fields on `PICSection`.
- `src/OTGW-firmware/OTGW-firmware.h` — `handlePendingPicHttp()`/`queuePicUpdateCheck()`/
  `queuePicRefresh()` declarations.
- `src/OTGW-firmware/OTGW-firmware.ino` — `handlePendingPicHttp()` call added to `loop()`
  under `#if HAS_PIC`, next to `handlePendingUpgrade()`.
- `src/OTGW-firmware/data/index.js` — `pollPICUpdateCheck()` and `pollPICRefresh()`
  replace the inline blocking-fetch-then-render of the PIC tab.
- ADR-132 (the bridge), ADR-134 (the upgrade-action deferral mirrored), ADR-135 (the TWDT
  window the bounded timeouts fit under).

## Enforcement

This ADR has a real code surface (AC#2: no synchronous `HTTPClient` call must remain on
an async-reachable path for the PIC update-check or refresh). The rule, however, is a
**call-graph / reachability** property: "no blocking outbound network primitive on a path
reachable from an `AsyncWebServerRequest*` handler or the `processAPI` dispatch." That is
not expressible as a single forbidden line-symbol the way ADR-132's `httpServer.` /
`handleClient(` gate is, because `http.GET()`/`http.sendRequest()`/`http.writeToStream()`
are legitimate **inside** the loop-context worker (`handlePendingPicHttp`,
`checkforupdatepic`, `refreshpic`) and must stay there. A blunt `forbid_pattern` on those
symbols would false-positive on the very code that implements the fix.

So the honest fit is an **LLM-judge** advisory: a reviewer (or the adr-judge LLM pass)
checks that any newly added blocking outbound network call sits in loop()-context worker
code and is reached only via the queue, not directly from an async handler. The
declarative side that *is* cheaply checkable is already covered by ADR-132's gate (the
sync WebServer must not reappear), and the bounded-timeout invariant is anchored by the
named constants in the References. The maintainer ratifies or downgrades this block at
the Proposed checkpoint.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": true
}
```
