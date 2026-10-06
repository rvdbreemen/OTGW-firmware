---
id: "ADR-188"
title: "Cap the web connections that hold send data, so a request storm cannot starve the receive path"
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
  - "heap"
aliases:
  - "web connection cap"
  - "request storm deafness"
components:
  - "web admission gates (restAPI.ino)"
  - "platform TCP shim (platform_esp32.h)"
symbols:
  - "webConnCapExceeded"
  - "platformTcpTxHoldingOnPort"
  - "WEB_MAX_TX_CONNECTIONS"
  - "hd_webconn_503"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-188 Cap the web connections that hold send data, so a request storm cannot starve the receive path

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
    reason: "Maintainer accepted 2026-10-06 after the interleaved bench runs on alpha.417 (TASK-1162): deaf in 1 of 5 runs with the cap (7.2 s) against 4 of 6 without (up to 292 s); nominal load hd_webconn_503 = 0"
    changed_via: adr-kit lifecycle
```

## Context

Under a burst of web requests the OTGW32 can go deaf for minutes: ping and new TCP
(Transmission Control Protocol) connections fail, then it recovers without a reboot
(TASK-1162). The TASK-1162 diagnostics traced the mechanism. The memory sits in the lwIP
(lightweight IP stack) send queues of connections that were already admitted. Each
connection can queue up to `TCP_SND_BUF` (5744 B), and up to 16 can be active. The WiFi
receive allocation (about 2.3 KB) then fails, no ACK (acknowledgement) is received, and
the queued data never drains.

The existing in-flight gates (ADR-147, ADR-165) limit how many responses are being built,
not how many connections still hold queued send data. A refusal-by-headroom variant (A) and
an abort-on-ack-timeout variant (B) did not help (TASK-1162 notes, 2026-10-05).

Bench, OTGW32 combo, `refresh_storm.py --workers 2,4,6,8 --seed 1124`, deaf window = ping
and connect both failing for at least 5 s:
- On the alpha.412 tree: base deaf in 3 of 3 runs (322 to 542 s). A per-connection
  send-buffer cap (C) alone: 2 of 3 deaf. A connection count cap (D): 0 of 3; C plus D:
  0 of 3.
- On dev (alpha.416, which has the non-blocking MQTT write of ADR-186): deaf in 1 of 3 runs
  (270 s).

## Decision

Refuse a new web request with the existing cheap 503 while more than
`WEB_MAX_TX_CONNECTIONS` (4) connections on port 80 hold or can queue send data. Both the
REST (Representational State Transfer) gate and the static-file gate check it first.

- A connection counts when it is ESTABLISHED, or in any other active state with
  unsent or unacked segments. A closing connection whose queue still drains holds that
  memory; one with empty queues holds none.
- The `/ws` WebSocket clients share port 80 and stay open, so their count
  (`otLogWs.count()`) is subtracted.
- The request being admitted is one of the counted connections.
- The count is taken by a platform shim, `platformTcpTxHoldingOnPort()`, which walks
  `tcp_active_pcbs` under `LOCK_TCPIP_CORE()`.
- Refusals are counted in `hd_webconn_503` (device/info), separately from the in-flight gates.

No library is changed.

## Decision Contract

### Must

- The REST and static-file admission paths call `webConnCapExceeded()` before their own
  in-flight check and answer 503 when it is true.
- The count subtracts the connected `/ws` clients.
- Every refusal increments `state.heapdiag.iWebConn503Count`, exposed as `hd_webconn_503`.

### Must Not

- Must not count connections that hold no send data and are not ESTABLISHED.
- Must not change AsyncTCP, ESPAsyncWebServer or the lwIP configuration.

### Exceptions

- Building with `-DWEB_MAX_TX_CONNECTIONS=1000` disables the cap for A/B measurement.

### Verification

- Bench, 2026-10-06, OTGW32 combo, the same storm and probe harness, interleaved on the
  same alpha.417 tree:
  - cap off (`-DWEB_MAX_TX_CONNECTIONS=1000`) plus alpha.416, 6 runs: deaf 0, 0, 270, 292,
    21 and 253 s; storm verdict 3 PASS, 3 FAIL.
  - cap 4 (alpha.417), 5 runs: deaf 0, 7.2, 0, 0 and 0 s; storm verdict 5 of 5 PASS.
  - No reboot in any run.
- Nominal load on alpha.417: three browser-like clients, each with an open `/ws` and
  polling device/info and settings for 180 s. `hd_webconn_503` stayed 0. The 9 other 503s
  came from the existing ADR-165 in-flight gates (three tabs loading at once exceed their
  cap of 2), not from this cap.
- Builds: esp32, esp32-classic and esp32-combo SUCCESS (alpha.417).

## Alternatives Considered

- **Count every active connection (variant D as first tested).** It removed the deafness,
  but a lone request right after a storm still got 503 in 3 of 6 runs, from closing
  connections with empty queues.
- **Count only ESTABLISHED connections.** It would admit new work while closing
  connections still hold queued send data, which is the memory being protected.
- **Cap the per-connection send buffer (variant C).** Deaf in 2 of 3 runs; not enough alone.
- **Change the lwIP or WiFi buffer configuration.** It needs a custom framework build and
  affects every connection, including MQTT (Message Queuing Telemetry Transport).

## Consequences

**Positive:**

- A request storm is refused cheaply instead of making the device deaf for minutes.
- The refusals are visible in `hd_webconn_503`.

**Negative:**

- Under a storm, clients get more 503s (about 1500 to 4700 per run on the bench). The web
  UI retries 503s through its fetch queue (ADR-184).
- The admission path takes the lwIP core lock once per request. It is a walk over at most
  16 connections; the bench runs showed no stall from it.
- One short deaf window (7.2 s) remained in 1 of 5 cap runs, so the cap reduces the
  problem, it does not remove every case.

## Open Questions

- None.

## Related Decisions

- ADR-147 and ADR-165 (in-flight gates for REST and static files), unchanged; this cap runs
  before them.
- ADR-186 (non-blocking MQTT write), which removed the loop-task watchdog reset under the
  same storm (TASK-1208).
- ADR-184 (client fetch queue), which retries the 503s.

## References

- TASK-1162 notes: the mechanism, the variant runs and the dev runs.
- Bench data: `%LOCALAPPDATA%/OTGW-capture/task1162-diag/variants-CD/`, `dev-a416/` and
  `dev-a417/`.
