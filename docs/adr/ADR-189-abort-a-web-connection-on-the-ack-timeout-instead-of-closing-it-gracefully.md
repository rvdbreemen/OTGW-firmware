---
id: "ADR-189"
title: "Abort a web connection on the ack timeout instead of closing it gracefully"
status: "Accepted"
date: "2026-10-06"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "web-server"
  - "backpressure"
  - "lwip"
aliases:
  - "abort on ack timeout"
  - "mid-body stall"
components:
  - "web request lifecycle (webServerCompat.h)"
symbols:
  - "webBeginRequest"
  - "hd_weback_abort"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-189 Abort a web connection on the ack timeout instead of closing it gracefully

## Status

Accepted, 2026-10-06.

## Status History

```yaml
status_history:
  - date: 2026-10-06
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-06
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: "Maintainer accepted 2026-10-06 after alpha.418 bench runs (TASK-1162): 0 mid-body stalls and 0 deaf windows in 5 storm runs, against 2 stalls with the cap alone and 17 without"
    changed_via: adr-kit lifecycle
```

## Context

TASK-1162 asks that under the 8-worker request storm no static response stalls mid-body:
each one is served complete, refused with a 503 before the body, or ends in a connection
abort the client sees at once.

ADR-188 (web connection cap) removed most of the deafness, but some responses still stopped
mid-body and left the client waiting for its own timeout (10 to 20 s). On the bench, counted
from the storm logs as exchanges ending in `timeout` during the body:
- without the cap, 6 runs: 17;
- with the cap (alpha.417), 5 runs: 2.

The mechanism is in the library defaults. AsyncTCP calls the request's timeout callback when
a response gets no ACK (acknowledgement) for `CONFIG_ASYNC_TCP_MAX_ACK_TIME` (5 s).
ESPAsyncWebServer then calls `close()`, which is lwIP (lightweight IP stack) `tcp_close()`.
That queues the FIN (TCP finish flag) behind the unsent body. Under memory pressure that body
cannot drain, so the client sees a body that stops and nothing else.

## Decision

`webBeginRequest()` replaces the request's timeout callback with one that calls
`AsyncClient::abort()` (`tcp_abort()`). That frees the queued send data at once and sends a
RST (TCP reset), which the client sees immediately. Each abort is counted in
`hd_weback_abort` (device/info). No library is changed.

## Decision Contract

### Must

- Every web request admitted through `webBeginRequest()` has an ack-timeout handler that
  aborts the connection.
- Each such abort increments `state.heapdiag.iWebAckAbortCount`.

### Must Not

- Must not change the ack timeout value or patch AsyncTCP or ESPAsyncWebServer.

### Exceptions

- None.

### Verification

- Bench, 2026-10-06, OTGW32 combo alpha.418 (ADR-188 cap plus this abort), the same storm
  and probe harness as ADR-188, 5 runs: deaf windows 0 in all, mid-body stalls 0, storm
  verdict 5 of 5 PASS, 2 aborts in total. The one incomplete exchange the client logged ended
  as `reset` during the body after 5.9 s, not as a timeout.
- Nominal load (three browser-like clients with `/ws`, 180 s) right after boot:
  `hd_webconn_503` stayed 0. Run right after five storms instead, the cap refused 3 of 219
  requests while the ADR-165 gates refused 10; the client queue (ADR-184) retries both.
- Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.418).

## Alternatives Considered

- **Keep the library's graceful close.** It leaves the client waiting for its own timeout,
  which is the stall TASK-1162 forbids.
- **Lower the ack timeout.** It would cut the wait but would also abort slow but healthy
  clients sooner, and it needs a library setting per connection.

## Consequences

**Positive:**

- A response that cannot finish ends with a RST the client sees at once, and its queued send
  memory is freed immediately instead of after retransmissions.

**Negative:**

- A client on a slow link that misses ACKs for 5 s gets a reset instead of a late
  completion. That was already a failed response under the old close.

## Open Questions

- None.

## Related Decisions

- ADR-188 (web connection cap), which this completes for TASK-1162.
- ADR-184 (client fetch queue), which retries failed requests.

## References

- TASK-1162 notes and bench data under `%LOCALAPPDATA%/OTGW-capture/task1162-diag/dev-a418/`.
