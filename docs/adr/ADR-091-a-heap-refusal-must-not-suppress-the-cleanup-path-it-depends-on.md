---
id: "ADR-091"
title: "A heap refusal must not suppress the cleanup path it depends on"
status: "Accepted"
date: "2026-08-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "heap"
  - "http"
  - "back-pressure"
  - "esp8266"
aliases:
  - "canServeHttp latch"
  - "HTTP serve gate self-deadlock"
  - "reapPendingHttpConnections"
components:
  - "HTTP loop-pump heap gate"
symbols:
  - "canServeHttp"
  - "reapPendingHttpConnections"
  - "HTTP_SERVE_MIN_MAXBLOCK"
context_scope: "selective"
format: "madr"
---

<!-- markdownlint-disable MD025 -->

# ADR-091 A heap refusal must not suppress the cleanup path it depends on

## Status

Accepted, 2026-08-25.

## Status History

```yaml
status_history:
  - date: 2026-08-25
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-08-25
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer after grilling. OTA scope split to its own ADR (TASK-1089); the backlog-slot question moved to Consequences as a bench-verified risk rather than an open decision.
    changed_via: adr-kit lifecycle
```

## Context and Problem Statement

This firmware carries two distinct HTTP heap gates, and the decision record
describes only one of them.

The documented one is `streamFileGuarded()`
(`src/OTGW-firmware/FSexplorer.ino:69-84`), which answers `503 Service
Unavailable` when the largest contiguous block is too small to stream a file.
ADR-086 refers to exactly this behaviour: "an existing heap-fragmentation gate
that answers `503 Service Unavailable` when the largest contiguous block falls
below a threshold" (`docs/adr/ADR-086-rate-limit-ui-polled-rest-endpoints.md:41`).

The undocumented one is `canServeHttp()`
(`src/OTGW-firmware/helperStuff.ino:1118`), which does not answer anything. It
withholds the call to `httpServer.handleClient()` from the main loop entirely
while heap health is not `HEAP_HEALTHY` **and** the largest contiguous block is
below `HTTP_SERVE_MIN_MAXBLOCK`. It shipped under TASK-841 with no ADR, and no
Accepted ADR names it: ADR-030 lists only WebSocket and message-queue telemetry transport (MQTT) as gated consumers,
ADR-083 (Proposed) likewise, and the only ADR that mentions `canServeHttp` by
name is ADR-084, which is Rejected.

That second gate has a defect of shape, not of threshold. `handleClient()` is
the only code that drains the web server's unclaimed-connection queue, and a
pending connection releases its receive buffers only when its reference count
reaches zero. Nothing other than `handleClient()` ever takes that reference. So
while the gate is shut, every pending connection keeps its protocol control
block and its receive buffers, and the contiguous block the gate is waiting for
cannot come back. The gate holds itself shut.

Field evidence, martreides 1.7.1 capture `otgw-171-2.log` (TASK-1037,
TASK-1039): once engaged, the largest block oscillates between 480 and 1872
bytes and never crosses the threshold again for the rest of the run. The skip
counter climbs 279, 4430, 9678, 14902 in roughly 40 seconds, which is loop ticks
at the measured rate of about 365 per second, not requests.

The obvious repair does not work. Forcing a `handleClient()` pass every N
milliseconds would run the multipart parser, which performs an unchecked
allocation of a 2100-byte contiguous `HTTPUpload` structure
(`ESP8266WebServer/src/Parsing-impl.h:475`, `HTTP_UPLOAD_BUFLEN 2048` plus
struct overhead). That is **larger** than `HTTP_SERVE_MIN_MAXBLOCK` itself, so
there is no band below the gate in which running the pump is safe.

## Decision Drivers

* A refusal that removes the only mechanism able to end the condition it is
  refusing over is a latch, not back-pressure. This is a property of the
  topology, independent of where the threshold sits.
* The device must remain reachable. A latched device also cannot be recovered
  over the air, because the flash-upload handlers are reached through the same
  gated call.
* Any repair runs in a cooperative loop that must keep feeding the watchdog, so
  it may not add unbounded work per tick.
* TASK-841 shipped this gate against a real fault under browser load. Whatever
  replaces the current behaviour must not reintroduce that fault.

## Considered Options

* **Option A — Release pending connections without serving them.** Keep
  withholding the pump, but pop and release the queued connections directly, at
  a bounded cadence, using no handler code and no allocation.
* **Option B — Force a full `handleClient()` pass after N consecutive skips.**
  An escape hatch that lets the normal path run occasionally.
* **Option C — Raise `HTTP_SERVE_MIN_MAXBLOCK`.** Give the gate more headroom.
* **Option D — Do nothing.** Treat the latch as acceptable, on the grounds that
  it is only reached during a heap collapse that has other causes.

## Decision Outcome

Chosen option: **Option A**.

The general rule this records, and the reason it is an architectural decision
rather than a tuning change: **a component that refuses work under resource
pressure must not, by refusing, disable the path that releases the resource.**
If refusing and releasing share a single entry point, they have to be separated
before the refusal is safe.

Applied here: `canServeHttp()` keeps deciding whether to serve, and a separate
`reapPendingHttpConnections()` releases queued connections while it says no. The
reaper runs no handler, parses nothing and allocates nothing, so it is safe at
any block size, which is precisely what Option B is not.

### Confirmation

With the gate engaged under synthetic load, the skip counter stops being
monotonic (the gate reopens at least once), the reaped counter advances
alongside it, and a request issued during a gated window is closed promptly
instead of hanging until the client times out.

## Decision Contract

### Must

* Keep the release path free of allocation and of handler code, so that it is
  valid at any largest-block size.
* Bound how often the release path runs, so it cannot starve the cooperative
  loop.
* Count the gate's activity in units that are honest about what they measure.
  The existing counter records loop ticks, not requests, and must be named and
  typed accordingly.

### Must Not

* Force the full request pump through as a way of escaping the gate. Its
  multipart parser makes an unchecked contiguous allocation larger than the gate
  threshold, so there is no block size at which that is safe.
* Raise `HTTP_SERVE_MIN_MAXBLOCK` as a remedy for the latch. A higher threshold
  makes the gate engage sooner, so it makes the latch more reachable, not less.
* Let the release path block. Closing a connection through a flushing call can
  wait hundreds of milliseconds inside the loop.

### Exceptions

* None.

### Verification

* `src/OTGW-firmware/helperStuff.ino` — `canServeHttp()` and the reaper beside
  it.
* `src/OTGW-firmware/OTGW-firmware.ino` — the loop call site, where refusing to
  serve now selects the reaper rather than doing nothing.

## Consequences

### Positive

* The gate can reopen. Refusal becomes genuine back-pressure rather than a
  one-way latch.
* Clients get a prompt connection close instead of an indefinite hang, so a
  browser or Home Assistant retries rather than stalling.
* The rule generalises: any future consumer gate can be checked against it.

### Negative

* **This alone does not restore the largest block in the captured scenario, and
  the record should not pretend otherwise.** At the fragmentation ratio measured
  in TASK-1037, a largest block below the threshold implies only about 2.1 KB
  free in total, so the withheld memory is referenced rather than fragmented.
  The latch stops; the underlying exhaustion is TASK-1037's leak and is fixed
  elsewhere. *Mitigation:* the acceptance criterion for block recovery is
  explicitly dependent on TASK-1037 rather than claimed here.
* A request arriving during a gated window is closed without a response. That is
  a deliberate trade against the previous behaviour of leaving it pending, and
  it is visible to users as a failed request rather than a slow one.
* **A second latch may exist one layer down, and this decision does not resolve it.**
  A pending connection whose peer has already closed its side (a TCP FIN) can leave a null protocol
  control block, and the accept path returns a backlog slot only when that block is
  present. If those slots are not returned, port 80 goes deaf while the largest block
  still looks healthy, which would be invisible to every counter this decision adds.
  It could not be settled from source: lwIP ships prebuilt as a static library and the
  relevant file is not in the tree. *Mitigation:* it is an observation rather than an
  open decision, since Option A is correct either way, so it is verified on the bench
  instead of blocking this record. The check is that a fresh request still receives a
  SYN-ACK after a long gated window; count successful accepts, not only block size.
  Tracked in TASK-1039 verification.
* The loop does slightly more work while the gate is shut, bounded by the
  cadence and by the small backlog limit.

### Neutral

* The threshold is unchanged. This decision is about topology; threshold values
  remain tunable without an ADR, which is the precedent this tree already
  follows.

## Open Questions

- [x] Should the reaper also apply when the gate is open, or is the shut-gate case the only one where the queue can grow unattended? — **Answered 2026-08-25 by User: Robert van den Breemen:** Shut-gate only. Established from the web server source rather than asked: handleClient() calls the accept path whenever it is not already mid-request (ESP8266WebServer-impl.h:309-311), so an open gate claims one pending connection per loop tick and the queue drains on its own. The reaper is therefore correctly bound to the refusal branch, and running it while the gate is open would duplicate work the pump already does.
- [x] A latched device cannot be recovered over the air today, because the flash-upload handlers are reached through the same gated call. Should the over-the-air update (OTA) path get an entry point independent of this gate, and does that belong in this decision or its own? — **Answered 2026-08-25 by User: Robert van den Breemen:** Its own ADR, tracked as TASK-1089. This decision records one rule: a refusal must not suppress its own cleanup path. Privileging OTA under heap pressure is a different promise with opposite trade-offs, since that path should be favoured rather than bounded, and merging the two would blur both while holding this one back for bench work that does not exist yet. The finding is real and confirmed: the comment claiming the flash-upload handlers are not gated overstates the guarantee, because the flags those handlers switch on are set by an upload handler that is itself only reachable through the gated call.

## Related Decisions

* **ADR-086 (Rate-Limit the UI-Polled REST Endpoints)**: describes an HTTP
  heap gate that answers 503. That description fits `streamFileGuarded()`, not
  the loop-pump gate this decision covers. Recording the distinction is part of
  the point.
* **ADR-030 (Heap Memory Monitoring and Emergency Recovery)**: defines the
  health tiers this gate reads. It names WebSocket and MQTT as gated consumers
  and does not cover HTTP.
* **ADR-079 (Emergency Heap Recovery Actions)**: the recovery path that runs at
  the critical tier, and a separate concern from this gate.

## References

* TASK-1039 — the defect this decision resolves.
* TASK-841 — introduced the gate; the browser-load fault it fixed must not
  regress.
* TASK-1037 — the leak whose terminal collapse is the window in which the latch
  was captured.
* `src/OTGW-firmware/helperStuff.ino:1118` — `canServeHttp()`.
* `src/OTGW-firmware/OTGW-firmware.ino` — the gated loop call site.
* `src/OTGW-firmware/FSexplorer.ino:69-84` — `streamFileGuarded()`, the 503 gate
  ADR-086 actually describes.
* `ESP8266WebServer/src/Parsing-impl.h:475` — the unchecked 2100-byte allocation
  that rules out forcing the pump.
* martreides 1.7.1 capture `otgw-171-2.log` — the field evidence.
