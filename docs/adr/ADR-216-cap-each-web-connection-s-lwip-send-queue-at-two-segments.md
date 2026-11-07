---
id: "ADR-216"
title: "Cap each web connection's lwIP send queue at two segments"
status: "Accepted"
date: "2026-11-07"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-165"
  - "ADR-188"
  - "ADR-212"
  - "ADR-215"
topics:
  - "web-server"
  - "heap"
  - "lwip"
  - "backpressure"
aliases:
  - "send buffer cap"
  - "snd_buf cap"
  - "two-tab heap dip"
components:
  - "platformTcpCapSendBuffer (platform_esp32.h)"
  - "WEB_TCP_SEND_CAP (webServerCompat.h)"
symbols: []
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-216 Cap each web connection's lwIP send queue at two segments

## Status

Accepted, 2026-11-07.

## Status History

```yaml
status_history:
  - date: 2026-11-07
    status: Proposed
    changed_by: Claude (TASK-1673)
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-07
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-165
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-188
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-212
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-215
    changed_via: adr-kit lifecycle
  - date: 2026-11-07
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted in session 2026-11-07 after the TASK-1673 bench A/B (median min heap 3.3 to 14.5 KB, +0.3 s load)
    changed_via: adr-kit lifecycle
```

## Context

Two browser tabs loading the Web UI at the same moment drove the free heap of an ESP32-S3 (Espressif dual-core microcontroller) bench unit without PSRAM (pseudo-static random-access memory) to about 3 KB (TASK-1673). A dip snapshot taken by the loop task at the lowest free heap (`hd_dip_*` in `/api/v2/device/info`) showed two port-80 connections each holding a full lwIP (lightweight IP stack) send queue: 2 x 5,744 bytes (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT`), plus 13 TIME_WAIT connections (TCP, Transmission Control Protocol). The server streams at most two files at a time (ADR-165) and closes a connection before its last data is acknowledged, so both streams fill their queue.

`TCP_SND_BUF` is compiled into the prebuilt lwIP library; a build flag cannot change it, and a rebuilt framework is a tooling change. lwIP treats `snd_buf` as a running credit: each write subtracts from it and each acknowledgement adds back only the acknowledged bytes. Lowering the credit once, before anything is queued, therefore caps that connection for its whole life without touching the library.

## Decision

`webBeginRequest()` (`src/OTGW-firmware/webServerCompat.h`) calls `platformTcpCapSendBuffer(pcb, WEB_TCP_SEND_CAP)` for every port-80 request. Under the TCP/IP core lock, and only while the connection has no unsent or unacknowledged data, the shim (`src/libraries/Platform/src/platform_esp32.h`) lowers `pcb->snd_buf` to `WEB_TCP_SEND_CAP` = 2,872 bytes (2 x MSS, maximum segment size). 2,872 is also the floor: ESPAsyncWebServer's acknowledgement path defers sending while its 2,872-byte response copy buffer does not fit in the free credit, so a lower cap would stall a response.

## Decision Contract

### Must

- Apply the cap before the first byte of a response is queued, under `LOCK_TCPIP_CORE`.
- Keep `WEB_TCP_SEND_CAP` at or above 2 x `CONFIG_LWIP_TCP_MSS` (2,872 bytes).
- Keep the change in the Platform shim and the web compatibility layer; no edit of the vendored AsyncTCP, ESPAsyncWebServer or lwIP code.

### Must Not

- Raise `snd_buf` above the value lwIP gave the connection.
- Change the cap on a connection that already has data queued.

### Exceptions

- None.

### Verification

- `tests/test_web_file_scheduling.py` compiles the real `webBeginRequest()`; its fixture stubs the shim, so it shows the call compiles and runs, not that lwIP honours the cap. That is shown only on the bench.
- Bench recipe (TASK-1673 notes): reboot before every run, two-tab load 25 rounds, read `hd_min_free_heap` and `hd_dip_tx_bytes` / `hd_dip_tx_pcbs`. With the cap, two overlapping streams hold 5,744 bytes together instead of 11,488.

## Alternatives Considered

- **One file stream at a time (`WEB_FILE_MAX_INFLIGHT` 1).** Worst case rose from about 3.0 to 7.6 KB, but one of four runs had a p95 tab load of 11.6 s. It serialises every asset and reverses ADR-165.
- **Rebuild the framework with a smaller `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`.** Affects every TCP connection (MQTT (Message Queuing Telemetry Transport), telnet, the PIC (Microchip microcontroller) bridge) and is a build-tooling change.
- **Accept the dip.** The unit never rebooted or answered 503 under the test load, but the margin for a concurrent allocation (MQTT burst, SAT, Smart Autotune Thermostat) was about 3 KB.

## Consequences

**Positive:**

- On the bench (six runs with the cap, three without, reboot per run), the median minimum free heap rose from about 3.3 KB to about 14.5 KB, and runs below 4 KB went from 2 of 3 to 1 of 6.

**Negative:**

- Page loads are about 0.3 s slower (p50 1.4 to 1.5 s against 1.1 to 1.3 s), because each connection has half the data in flight.
- The worst case is not bounded: one run still reached 3,252 bytes. The remaining transient (about 10 KB below what the loop task sees) sits on the async_tcp task outside the send queues and is tracked separately. The maintainer waived TASK-1673's 8 KB target on 2026-11-07 on these numbers.
- The cap relies on lwIP's credit semantics for `snd_buf`; a framework update that changes them needs the bench recipe re-run.

## Open Questions

- [x] Accept this decision (maintainer). — **Answered 2026-11-07 by Robert van den Breemen:** Accepted by Robert in the session on 2026-11-07 after the TASK-1673 bench A/B.

## Related Decisions

- ADR-165 (two file streams), ADR-188 (connection cap), ADR-212 (file queue), ADR-215 (one gzipped file per page).

## References

- TASK-1673 notes: dip snapshot, cap-1 and cap-2872 A/B results.
- `src/libraries/Platform/src/platform_esp32.h` (`platformTcpCapSendBuffer`), `src/OTGW-firmware/webServerCompat.h` (`webBeginRequest`).
