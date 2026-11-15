---
id: "ADR-240"
title: "A web connection that is not ESTABLISHED and holds no payload does not count towards the ADR-188 send cap"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "web server"
  - "connection cap"
  - "503"
  - "TCP"
  - "lwIP"
  - "fresh page load"
aliases:
  - "WEB CONN CAP"
  - "webConnCapExceeded"
  - "hd_webconn_503"
  - "FIN-only connection"
  - "closing-fin"
  - "syn-rcvd"
components:
  - "platformTcpTxHoldingOnPort"
symbols:
  - "platformTcpTxHoldingOnPort"
  - "platformTcpTxBreakdownOnPort"
  - "webConnCapExceeded"
  - "WEB_MAX_TX_CONNECTIONS"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-240 A web connection that is not ESTABLISHED and holds no payload does not count towards the ADR-188 send cap

## Status

Accepted, 2026-11-15. Amends ADR-188 in one counting rule; ADR-188 stays Accepted and unchanged otherwise.
Guideline-level per ADR-080: verified by the host test named below, no evaluate.py gate.

Decision Maker: Robert van den Breemen (maintainer), Discord DM 2026-11-15, relayed by the coordinator: "A" on the
TASK-1947 question (A = this ADR, B = keep ADR-188 as it is), message 1571427079680041031, and "A" again on both
corrections of that question (1571427204842266678, 1571427621433118872).

## Status History

```yaml
status_history:
  - date: 2026-11-15
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-15
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Maintainer chose A for TASK-1947 (Discord DM 1571427079680041031, confirmed on both corrections 1571427204842266678 and 1571427621433118872)
    changed_via: adr-kit lifecycle
```

## Context

ADR-188 refuses a new web request with a cheap 503 while more than `WEB_MAX_TX_CONNECTIONS` (4) connections on port
80 hold or can queue send data (the WebSocket clients subtracted). Its counting rule: "A connection counts when it is
ESTABLISHED, or in any other active state with unsent or unacked segments." The shim
`platformTcpTxHoldingOnPort()` (`src/libraries/Platform/src/platform_esp32.h`) walks `tcp_active_pcbs` and counts
exactly that.

lwIP (lightweight IP, the device's network stack for TCP (Transmission Control Protocol) over IP) queues two control segments with no payload (`tcp_seg.len` is 0; lwIP's
`TCP_TCPLEN` adds 1 only for the SYN (synchronise) or FIN (finish) flag, `tcp_priv.h`):

- the FIN (the TCP close marker) of a closing connection. The web server (ESPAsyncWebServer 3.11.2) answers every
  request with `Connection: close` (`WebResponses.cpp` lines 270 and 340 of the library), so a page load opens a new
  TCP connection per request and closes it again. After a response the connection
  sits in FIN_WAIT_1, CLOSING or LAST_ACK with only that FIN queued until the peer acknowledges it.
- the SYN-ACK (the server's synchronise plus ACK (acknowledgement)) of a passive open. `tcp_listen_input()` sets a new connection to SYN_RCVD, registers it in
  `tcp_active_pcbs` and queues its SYN-ACK with `tcp_enqueue_flags(SYN|ACK)` (ESP-IDF (Espressif Internet-of-Things Development Framework) lwIP in framework-espidf
  3.50501: `tcp_in.c` lines 693, 714 and 735; `tcp_out.c` line 1036 allocates only the option bytes). Such a
  connection has received no request yet; it becomes ESTABLISHED when the client's ACK arrives.

Under ADR-188 both count as holding send data, although neither holds a payload byte or a request.

Evidence (TASK-1947):

- 09 on `.63` (beta.250) and `.61` (beta.252): fresh web UI (user interface) loads got 503s on the first API (application programming interface) calls while the browser
  had at most 2 API requests open (Chrome DevTools Protocol request timeline).
- 09 on `.61` (beta.264+a695744, 2026-11-15) with the first TASK-1947 diagnostic line: 20 fresh loads, 9 of them with
  503s, 36 refusals (`hd_webconn_503` 0 to 36). In every refusal 2 to 5 of the counted connections were
  non-ESTABLISHED with only zero-length segments queued (the line's `closing-fin` bucket, which then also held
  SYN_RCVD connections), `closing-data` was always 0, `est-data` 0 to 1, `est-idle` 1 to 3 including the WebSocket
  client. Examples: 10x `6 connections hold TX (1 ws) [est-idle 2 est-data 1 closing-data 0 closing-fin 3, 228 B
  queued]`, 8x `[3/0/0/3, 0 B]`, 1x `7 (1 ws) [2/1/0/4]`. Evidence in 09's scratchpad `s1780/i5/` (telnet raw and
  the per-load timelines).
- Without the zero-length-only connections every one of the 36 refusals would have been at most 3 after the
  WebSocket subtraction, under the cap of 4. That holds whichever of the two states filled the bucket.
- The diagnostic line now names `closing-fin` (FIN_WAIT_1, CLOSING, LAST_ACK), `syn-rcvd` and `other` apart, so the
  next bench shows which state the refused loads hit.

## Decision

A connection on port 80 that is not ESTABLISHED counts towards the ADR-188 cap only while at least one queued
segment (unsent or unacked) carries payload; a closing connection with only its zero-length FIN and a half-open
SYN_RCVD connection with only its zero-length SYN-ACK do not count.

Everything else in ADR-188 stays: the cap of 4, the WebSocket subtraction, ESTABLISHED connections counting with or
without queued data (except the native file-queue waiters), the 503 answer and the `hd_webconn_503` counter.

## Decision Contract

### Must

- `platformTcpTxHoldingOnPort()` counts a non-ESTABLISHED connection only when a segment in `unsent` or `unacked`
  has `len != 0`.
- An ESTABLISHED connection keeps counting as before: with queued segments, or without them unless it is a native
  file-queue waiter. A request that is being admitted is ESTABLISHED and counts.
- The refusal line keeps the zero-length-only connections apart by state (`closing-fin`, `syn-rcvd`, `other`).

### Must Not

- Must not stop counting idle ESTABLISHED connections: a request whose handler has not answered yet is idle at
  admission, so a storm of new requests would all be admitted and then fill their send queues at once, which is the
  starvation ADR-188 prevents. A SYN_RCVD connection that completes its handshake becomes ESTABLISHED and is counted
  from then on, so this guard also covers it.
- Must not change `WEB_MAX_TX_CONNECTIONS`.

### Exceptions

- None.

### Verification

- `tests/test_web_conn_cap_states.py`: compiles the real shim against a stub list of pcbs (lwIP protocol control blocks); a FIN-only FIN_WAIT_1,
  CLOSING and LAST_ACK connection, a SYN_RCVD connection with only its SYN-ACK and another state with only a
  zero-length segment count 0; a closing connection with payload counts 1, ESTABLISHED idle and with data count 1, a
  queue waiter 0; the modelled fresh load (4 ESTABLISHED + 2 zero-length-only) counts 4; the breakdown names each
  state. Red on the shim of ADR-188 (3 failures: zero-length-only counted 1, the model 6), green on this rule.
- Bench: 20 fresh loads on `.61` with the diagnostic line show 0 refusals caused by zero-length-only connections,
  and report `closing-fin` against `syn-rcvd` (TASK-1947 AC3).

## Alternatives Considered

- **Keep ADR-188 as is and change the web UI load order** (load the page first, wait, then start the API calls).
  Rejected as the recommendation: it hides the counting error for one client only, a second browser or a script
  still hits it, and it slows every page load.
- **Exclude only the closing states (FIN_WAIT_1, CLOSING, LAST_ACK) and keep counting SYN_RCVD.** Rejected: the
  beta.264 bucket did not separate the two states, so this could leave the measured refusals in place; and a
  SYN_RCVD connection holds no request and no payload either, and counts as soon as it is ESTABLISHED.
- **Also stop counting idle ESTABLISHED connections.** Rejected: see Must Not; 09's numbers show the zero-length rule
  alone brings every measured refusal under the cap.
- **Raise the cap above 4.** Rejected: the cap protects the receive path under a request storm (ADR-188), and a
  higher cap also admits more connections that really hold send data.

## Consequences

**Positive:**

- A fresh web UI load no longer gets 503s from connections that hold neither payload nor a request (36 refusals in
  20 loads on the bench, all with 2 to 5 zero-length-only connections).
- The cap keeps its protection: every connection that holds payload, and every ESTABLISHED one, still counts.

**Negative:**

- A closing connection whose FIN is in flight, or a half-open one waiting for the client's ACK, still needs an lwIP
  pcb and a little memory; it is no longer counted. Mitigation: it holds no payload bytes (`0 B queued` in the
  diagnostic), and the number of pcbs is bounded by lwIP's own limit, not by this cap. A SYN flood is outside
  ADR-188's scope (the gateway serves the LAN (local area network) only) and is bounded by the same lwIP limit.
- The shim walks the segment list of non-ESTABLISHED connections, under the core lock that it already holds. Cost:
  a few segments per connection, only on the admission path.

## Open Questions

- [x] Maintainer choice for TASK-1947: A (this ADR) or B (keep ADR-188, no change). Answered A on 2026-11-15 (see Status).

## Related Decisions

- **ADR-188 (Cap the web connections that hold send data)**: amended by this ADR in one counting rule; ADR-188 stays
  Accepted and the rest of it stands unchanged.
- **ADR-212 (Load the Web UI assets natively behind a bounded server-side file queue)**: its file-queue waiters stay
  excluded from the count.
- **ADR-184 (Queue the classic UI's API requests at two in flight)**: the UI side of the measurement (at most 2 API
  requests open).

## References

- TASK-1947 (backlog), notes of 2026-11-14 and 2026-11-15 (research, diagnostic, bench breakdown, the state split
  after the second-reader review).
- `src/libraries/Platform/src/platform_esp32.h`, `platformTcpTxHoldingOnPort()` and `platformTcpTxBreakdownOnPort()`.
- `src/OTGW-firmware/restAPI.ino`, `webConnCapExceeded()`.
- `tests/test_web_conn_cap_states.py`.
- ESPAsyncWebServer 3.11.2, `src/WebResponses.cpp` (the `Connection: close` header).
- ESP-IDF lwIP (framework-espidf 3.50501): `src/core/tcp_in.c` `tcp_listen_input()`, `src/core/tcp_out.c`
  `tcp_enqueue_flags()`, `src/include/lwip/priv/tcp_priv.h` (`TCP_TCPLEN`, `struct tcp_seg`). Single source: the
  Arduino core links a prebuilt lwIP from the same ESP-IDF line; not checked against that binary.
