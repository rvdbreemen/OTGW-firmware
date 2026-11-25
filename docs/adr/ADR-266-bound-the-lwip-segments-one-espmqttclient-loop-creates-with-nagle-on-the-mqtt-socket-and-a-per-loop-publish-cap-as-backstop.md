---
id: "ADR-266"
title: "Turn Nagle on for the espMqttClient socket, so one MQTT loop hands lwIP 2-3 segments instead of 16"
status: "Accepted"
date: "2026-11-25"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "mqtt"
  - "espmqttclient"
  - "heap"
  - "lwip"
  - "burst-pacing"
aliases:
  - "MQTT segment cost"
  - "Nagle on MQTT socket"
  - "EMC_TCP_NODELAY"
  - "max packets per loop (not adopted)"
components:
  - "EMC_TCP_NODELAY (fork Config.h)"
symbols:
  - "EMC_TCP_NODELAY"
  - "ClientSync::connect"
  - "MqttClient::_checkOutbox"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-266 Turn Nagle on for the espMqttClient socket, so one MQTT loop hands lwIP 2-3 segments instead of 16

## Status

Accepted, 2026-11-25, by Robert van den Breemen: Nagle alone, the per-loop PUBLISH cap of the first version dropped (Open Question 1), accepted now with the remaining per-loop drops in TASK-3005 (Open Question 5). Proposed 2026-11-20, rewritten 2026-11-21 after the TASK-2949.5 bench.

**Decision Maker:** User: Robert van den Breemen.

- **What he asked for.** On 2026-11-20 he asked for a per-loop packet cap in our fork, for 2.0.0 (ADR-264 context: the cap "is conditional on a measurement").
- **What the bench showed.** Nagle alone takes the broker socket's send-queue high-water mark to 2-3 segments on both boards.
- **The recommendation.** The architect, with otgw-dev's agreement, recommended adopting Nagle and not the cap.
- **His choice.** On 2026-11-25 Robert chose Nagle alone and dropped the cap he had asked for (Open Question 1).

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Split out of ADR-264 (one decision per ADR); mechanism confirmed by otgw-dev's beta.403 diagnosis and source check
    changed_via: adr-kit
  - date: 2026-11-25
    status: Accepted
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Accepted by Robert van den Breemen, 2026-11-25, interactive in the coordinator session (relayed to otgw-architect)
    changed_via: adr-kit lifecycle
```

## Context

One `MQTTclient.loop()` writes the whole espMqttClient outbox. `_checkOutbox()` runs `while (_sendPacket() > 0)` until the socket refuses (`src/libraries/espMqttClient/src/MqttClient.cpp:354`). Each MQTT (Message Queuing Telemetry Transport) packet is its own non-blocking `::send()` (`Transport/ClientSync.cpp:60-74`, ADR-186). The library itself turns Nagle's algorithm off on connect: `TCP_NODELAY` is set at `ClientSync.cpp:34-36` (connect by IP) and `:52-54` (connect by host).

**Mechanism, confirmed.** otgw-dev measured it on beta.403 on .61 and checked it in the source on 2026-11-20, relayed by the coordinator:

- With Nagle off, every packet becomes its own TCP (Transmission Control Protocol) segment.
- `TCP_OVERSIZE` = MSS (maximum segment size, 1,436 B) (`lwipopts.h:639-640`, `CONFIG_LWIP_TCP_OVERSIZE_MSS 1`) gives each segment a pbuf (lwIP packet buffer) of about 1.5-1.6 KB, whatever the payload.
- `TCP_SND_QUEUELEN` = (4 x 5,744 + 1,435) / 1,436 = 16 segments (`lwip/opt.h:1400`, `TCP_SND_BUF` 5,744 at `lwipopts.h:627`, MSS at `:621`). So one `loop()` hands at most 16 packets to lwIP (the lightweight IP stack) and then gets `EAGAIN`.
- Measured: outbox 25 to 9, so 16 packets in one pass, cost 20.3-23.9 KB. The heap recovered within 14 ms, once the broker's ACK (acknowledgement) arrived.

The dominant cost is therefore about 1.5 KB per segment. A SAT (Smart Autotune Thermostat) topic of about 80 B pays the same 1.5 KB as a 1.4 KB discovery payload. Two runs (29.4 KB and 32.2 KB) are 4-8 KB higher than 16 segments explain. That remainder is a separate measurement (Open Question 2), not part of this decision.

Our fork is the library source (ADR-264), so the socket option and the flush loop are ours to set. The ADR-265 pacer limits how full the outbox gets for producers that can wait. It cannot bound what one `loop()` hands to lwIP, and it does not cover the event-driven publishers: the per-frame `processOT()` publishes and the status fan-outs.

## Decision

**Nagle on for the MQTT socket.**

- **The fork.** It gains `EMC_TCP_NODELAY` in `src/Config.h`. The default is 1, which keeps upstream's `TCP_NODELAY`.
- **The firmware.** It builds with 0, so `ClientSync::connect()` leaves Nagle on.
- **What lwIP does then.** It sends the first small packet at once. Later writes go into the segment that is still unsent, up to one MSS, until the broker's ACK comes back.
- **The effect.** A burst of small SAT packets becomes 2-3 segments instead of 16.

**The per-loop PUBLISH cap of the first version (`EMC_MAX_PACKETS_PER_LOOP`, `setMaxPacketsPerLoop`) is dropped (Robert, 2026-11-25, Open Question 1).**

- With Nagle alone, the broker socket's send-queue high-water mark is 2-3 segments on both boards (measured below).
- The cap would only bound bursts of large packets, and the bench did not show a failure that needs it (YAGNI).
- If a later measurement shows that need, it becomes its own ADR.

**Measured** (otgw-bench-tester on the NAS (network-attached storage) rig, 2026-11-21, beta.409+41827f9 and beta.410+eedf081 diag arms; arm identified by sha256; loopback otdmode 4, SAT on, satsimulation false, weather off, BLE (Bluetooth Low Energy) off, no telnet client, no `/ws` client (`hd_dip_ws` 0 in every run), broker on the NAS):

| quantity | TCP_NODELAY (today) | Nagle on | n, board |
|---|---|---|---|
| `hd_min_free_heap` | 728 / 844 / 2,736 B; repeat 1,632 / 2,444 / 6,312 B | 13,432 / 21,132 / 21,872 B; repeat 21,828 / 28,288 / 33,228 B | 3+3 per arm, .61 |
| broker socket send-queue high-water mark (segments) | 10 / 16 / 16 | 2 / 2 / 3 | 3 per arm, .61 |
| the same, as an allocation estimate | 15.4-24.7 KB | 3.1-4.6 KB | .61 |
| `hd_min_free_heap` | 23,468 / 24,116 / 25,848 B | 23,780 / 29,832 / 30,008 B | 3 per arm, .63 (PSRAM) |
| send-queue high-water mark | 16 | 3 | .63 |
| largest drop inside one `MQTTclient.loop()` (probe `at=13 from=12`), takeover / republish-all / discovery republish | 42.9 / 42.9 / 43.1 KB | 7.3 / 9.0 / 13.5 KB | 3 per arm, .61 |
| command echo (HA `set/` to its state on the broker), p50 / p95 per boot | 111.5-114.0 / 126.7-221.0 ms | 111.0-114.9 / 121.2-221.1 ms | 3 boots x 20 per arm, .61 |
| PINGREQ to PINGRESP (broker log) | under 1 s | under 1 s, no disconnect | 5 of 6 boots, .61 |

- **Echo spread.** The 221 ms p95 boot occurs once in each arm, so it is not bound to Nagle. One sample of 120 reached 251.4 ms (the max, Nagle arm).
- **Soak.** 60 min on .61 with Nagle on: no restart, MQTT connected throughout, heap flat. `hd_min` went once to 8,228 B at 14:48, cause not visible (no probe record at that moment). That minimum belongs to the build under test (bootcount 1166, beta.410+eedf081); an earlier 1,188 B row in the same `info.jsonl` is from the previous firmware.
- **Evidence paths** (each with SHA256SUMS), all under `OTGW-field-data`:
  - `bench-otgw32/2026-11-21_task2949-5-heapab/`;
  - `bench-otgw32/2026-11-21_task2949-5-rtt/`;
  - `bench-otgw32/2026-11-21_task2949-5-takeover/`;
  - `bench-otgw32/2026-11-21_task2949-5-soak/`;
  - `bench-63/2026-11-21_task2949-5-events/`.

**What this decision does not bound.**

- **Drops outside the MQTT loop.** With Nagle on, the largest drops are no longer in the MQTT loop. They are in the SAT publish pass (`satPublishMQTT`, 10-18.5 KB) and in `satControlLoopRun` (16-20 KB) at the takeover. Those are TASK-3005 and ADR-265's scope, not this ADR's.
- **On .63, per-loop drops of 8-14 KB** appear in both arms, so they are not bound to Nagle.

```cpp
// fork Transport/ClientSync.cpp, in both connect() overloads (ESP32 branch)
#if EMC_TCP_NODELAY
    int val = true;
    client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
#endif
```

## Decision Contract

### Must

- **Default.** `EMC_TCP_NODELAY` defaults to 1 (upstream behaviour), so the fork commit can go upstream unchanged.
- **One place.** The firmware sets `EMC_TCP_NODELAY=0` in one place (a `platformio.ini` build flag), with a comment citing this ADR and the TASK-2949.5 numbers.
- **ADR-264 process.** The fork change is its own fork commit, marked with a comment naming this ADR. It reaches the firmware the ADR-264 way: a new pinned fork tag, then the vendored copy updated to that tag. No hand edit in `src/libraries/espMqttClient`.

### Must Not

- Must not drop or reorder packets. The outbox stays first-in-first-out.
- Must not block the loop task: the non-blocking write of ADR-186 stays.
- If Robert chooses option 1(a): must not add the per-loop PUBLISH cap later without a new ADR and a measurement that shows the need.

### Exceptions

- None.

### Verification

- **Host test.** It shows `ClientSync::connect()` sets `TCP_NODELAY` only when `EMC_TCP_NODELAY` is 1: the stub records the socket option. It is red when the firmware build flag is missing.
- **Bench.** TASK-2949.5 on .61 and .63 (the table under Decision): broker socket send-queue high-water mark 2-3 with the firmware's value, and a command echo under 250 ms at p95.

## Alternatives Considered

- **A. Per-loop PUBLISH cap, alone or as a backstop.** It bounds about n x 1.5 KB per pass, for every packet type, but small packets still pay one segment each. Dropped by Robert on 2026-11-25 (Open Question 1): with Nagle alone the send-queue high-water mark is 2-3 on both boards (TASK-2949.5), so a backstop has no measured failure to stop (YAGNI). The cap variants were not benched.
- **B. Coalescing in the library through a staging buffer.** Copy several packets into one buffer of about one MSS and issue one `send()`. It has the same effect as Nagle, without the ACK wait, but it is new code on the hot path. Partial writes then span packet boundaries, and the buffer needs about 1.4 KB more RAM. `EMC_TX_BUFFER_SIZE` (1,440) exists, but it only serves chunked payload callbacks (`Packets/Packet.cpp:430`, `:438`), not coalescing. Not chosen: Nagle gets the same segment saving from code lwIP already has.
- **B2. `MSG_MORE` on every `send()` except the last of a pass.** With `TF_NODELAY`, lwIP still pushes each write out at once, so the merge into the unsent segment probably does not happen. This is not verified in Espressif's lwIP. Not chosen.
- **C. Do nothing in the library; ADR-265 pacer only.** The event-driven publishers stay unbounded and the 16-segment worst case remains. Not chosen.
- **Earlier objection to Nagle (ADR-265 and analysis, first version).** That version rejected Nagle on latency, before the cost per segment was known. Nagle delays the tail of a burst by at most one ACK round trip. With a LAN broker that is a few milliseconds. A delayed ACK on the broker's host can stretch this to about 40 ms (Linux) to 200 ms. For a heating gateway with a 1 s OpenTherm cadence that is acceptable, if Open Question 3 shows no stalls. Re-weighed here.

## Consequences

**Positive:**

- **The cost is attacked at its cause:** fewer segments, so less transient heap. On .61:
  - `hd_min_free_heap` went from 0.7-6.3 KB to 13.4-33.2 KB;
  - the largest drop in one MQTT loop went from about 43 KB to 7.3-13.5 KB.
- **Small change surface:** one build flag in the firmware and one small, opt-in fork commit that can go upstream.

**Negative:**

- **Latency.** Nagle can delay a reply by up to one delayed-ACK interval. Measured: no arm-bound difference in the command echo, and PINGRESP stayed under 1 s.
- **The 10 KB target is missed once.** The discovery republish on .61 still had a 13.5 KB drop inside one MQTT loop, above the 10 KB the first version aimed for. Robert accepted that on 2026-11-25 (AC2, Open Question 5); the remaining per-loop drops are TASK-3005's.
- **Remaining drops sit elsewhere.** The largest drops are now outside the MQTT loop (TASK-3005).
- **Still unexplained.** The 4-8 KB that the segment count did not explain in the first measurements (former Open Question 2) has its own follow-up, and the soak's single 8,228 B minimum has no probe record.

## Open Questions

- [x] 1. Robert's choice on the variant. The bench measured Nagle alone against today; the cap variants were not benched. The options:
  - (a) Nagle only, the cap he asked for on 2026-11-20 dropped (architect's recommendation, otgw-dev agrees);
  - (b) Nagle plus the cap as a backstop, which needs the cap built and benched first.
  — **Answered 2026-11-25 by Agent: Claude (otgw-architect):** Robert, 2026-11-25 (interactive in the coordinator session, relayed): (a) Nagle only. The per-loop cap he asked for on 2026-11-20 is dropped.
- [x] 2. Where do the extra 4-8 KB go in runs 2#2 and 4? — **Answered 2026-11-21 by Agent: Claude (otgw-architect):** not part of this decision. With Nagle on, the largest drops are outside the MQTT loop, and they are followed up in TASK-3005.
- [x] 3. With Nagle on, are command round-trip and SAT publish latency still acceptable: under 250 ms, and with no keepalive or PUBACK stall? — **Answered 2026-11-21 by Agent: Claude (otgw-architect), from TASK-2949.5:**
  - the command echo p95 was under 250 ms on 6 of 6 boots, with no arm-bound difference;
  - PINGRESP was under 1 s, with no disconnects (broker log);
  - device-side PINGREQ timing has no firmware counter, so it was measured broker-side only.
- [x] 4. The upstream PR text, after the bench, checked by the coordinator and Robert before it opens. It covers `EMC_TCP_NODELAY`, plus the cap if Robert chooses 1(b). — **Answered 2026-11-25 by Agent: Claude (otgw-architect):** The PR covers EMC_TCP_NODELAY only, as the cap is dropped (question 1). Opening it is a follow-up under the ADR-264 fork process; the coordinator and Robert check the text before it opens. Not a precondition of this decision.
- [x] 5. Acceptance by the maintainer, together with his AC2 choice:
  - (a) accept this ADR on what Nagle demonstrably does, with the per-loop allocation of `satPublishMQTT` and `satControlLoopRun` handled in TASK-3005 / ADR-265;
  - (b) hold it until TASK-3005 has landed.
  — **Answered 2026-11-25 by Agent: Claude (otgw-architect):** Robert, 2026-11-25 (interactive in the coordinator session, relayed): (a) accept now on what Nagle demonstrably does; the remaining per-loop drops (satPublishMQTT, satControlLoopRun) go to TASK-3005.

## Related Decisions

- **ADR-264** (espMqttClient from our fork): where these two changes live.
- **ADR-265** (shared MQTT pacer, Proposed): guards how full the outbox gets. This ADR guards the segments per pass.
- **ADR-186** (non-blocking write): unchanged. Its vendored `ClientSync` connect code gains the `EMC_TCP_NODELAY` switch.
- **ADR-131** (loop-task pumping): unchanged.
- **ADR-216** (web send-queue cap): a byte cap on `snd_buf`. It does not bound how many small segments are in flight.

## References

- `src/libraries/espMqttClient/src/MqttClient.cpp:271`, `:293`, `:354`, `:481`, `:530`; `Config.h:19-20`, `:32`; `Packets/Packet.cpp:430`, `:438`; `Transport/ClientSync.cpp:34-36`, `:52-54`, `:60-74`.
- `framework-arduinoespressif32-libs/esp32s3`: `lwipopts.h:621`, `:627`, `:639-640`; `lwip/opt.h:1400`.
- otgw-dev beta.403 diagnosis and source check (TASK-2019 notes, relayed by the coordinator, 2026-11-20); TASK-1757 probes; epic TASK-2949.
- TASK-2949.5 bench, 2026-11-21: the evidence folders listed under Decision; TASK-3005 (drops outside the MQTT loop).
