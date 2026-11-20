---
id: "ADR-266"
title: "Bound the lwIP segments one espMqttClient loop creates, with Nagle on the MQTT socket and a per-loop PUBLISH cap as backstop"
status: "Proposed"
date: "2026-11-20"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
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
  - "max packets per loop"
  - "setMaxPacketsPerLoop"
components:
  - "EMC_TCP_NODELAY (fork Config.h)"
  - "EMC_MAX_PACKETS_PER_LOOP / MqttClient::setMaxPacketsPerLoop (fork)"
symbols:
  - "EMC_TCP_NODELAY"
  - "EMC_MAX_PACKETS_PER_LOOP"
  - "setMaxPacketsPerLoop"
  - "MqttClient::_checkOutbox"
  - "ClientSync::connect"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-266 Bound the lwIP segments one espMqttClient loop creates, with Nagle on the MQTT socket and a per-loop PUBLISH cap as backstop

## Status

Proposed, 2026-11-20. Acceptance waits for the bench A/B in TASK-2949.5, which settles Open Question 1.

**Decision Maker:** User: Robert van den Breemen asked on 2026-11-20 for a per-loop cap in our fork, for 2.0.0. Which mechanism to use is the agent's recommendation, weighed below.

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Split out of ADR-264 (one decision per ADR); mechanism confirmed by otgw-dev's beta.403 diagnosis and source check
    changed_via: adr-kit
```

## Context

One `MQTTclient.loop()` writes the whole espMqttClient outbox. `_checkOutbox()` runs `while (_sendPacket() > 0)` until the socket refuses (`src/libraries/espMqttClient/src/MqttClient.cpp:354`). Each MQTT (Message Queuing Telemetry Transport) packet is its own non-blocking `::send()` (`Transport/ClientSync.cpp:60-74`, ADR-186). The library itself turns Nagle's algorithm off on connect: `TCP_NODELAY` is set at `ClientSync.cpp:34-36` (connect by IP) and `:52-54` (connect by host).

**Mechanism, confirmed.** otgw-dev measured it on beta.403 on .61 and checked it in the source on 2026-11-20, relayed by the coordinator:

- With Nagle off, every packet becomes its own TCP segment.
- `TCP_OVERSIZE` = MSS (maximum segment size, 1,436 B) (`lwipopts.h:639-640`, `CONFIG_LWIP_TCP_OVERSIZE_MSS 1`) gives each segment a pbuf (lwIP packet buffer) of about 1.5-1.6 KB, whatever the payload.
- `TCP_SND_QUEUELEN` = (4 x 5,744 + 1,435) / 1,436 = 16 segments (`lwip/opt.h:1400`, `TCP_SND_BUF` 5,744 at `lwipopts.h:627`, MSS at `:621`). So one `loop()` hands at most 16 packets to lwIP (the lightweight IP stack) and then gets `EAGAIN`.
- Measured: outbox 25 to 9, so 16 packets in one pass, cost 20.3-23.9 KB. The heap recovered within 14 ms, once the broker's ACK (acknowledgement) arrived.

The dominant cost is therefore about 1.5 KB per segment. A SAT (Smart Autotune Thermostat) topic of about 80 B pays the same 1.5 KB as a 1.4 KB discovery payload. Two runs (29.4 KB and 32.2 KB) are 4-8 KB higher than 16 segments explain. That remainder is a separate measurement (Open Question 2), not part of this decision.

Our fork is the library source (ADR-264), so the socket option and the flush loop are ours to set. The ADR-265 pacer limits how full the outbox gets for producers that can wait. It cannot bound what one `loop()` hands to lwIP, and it does not cover the event-driven publishers: the per-frame `processOT()` publishes and the status fan-outs.

## Decision

The decision has two parts.

**Primary measure: Nagle on for the MQTT socket.** The fork gains `EMC_TCP_NODELAY` in `src/Config.h`. The default is 1, which keeps upstream's `TCP_NODELAY`. The firmware builds with 0, so `ClientSync::connect()` leaves Nagle on. lwIP then sends the first small packet at once and merges the following writes into the segment that is still unsent, up to one MSS, until the broker's ACK. A burst of 25 small SAT packets becomes 2-3 segments (about 3-5 KB) instead of 16 (about 24 KB).

**Backstop: the per-loop PUBLISH cap.** The fork gains `EMC_MAX_PACKETS_PER_LOOP` in `src/Config.h`, plus a `setMaxPacketsPerLoop(n)` setter. The default 0 means unlimited, which is upstream behaviour. The firmware sets `n = 8`. With Nagle on, small packets no longer create a segment each, so the cap mainly limits bursts of large packets, such as discovery configs or PIC settings: 8 large packets cost about 12 KB, against 24 KB without the cap.

Rules for the cap:

- Only fully written PUBLISH packets count. PINGREQ (`MqttClient.cpp:481`), PUBACK (`:530`), PUBREC, PUBREL, PUBCOMP, SUBSCRIBE, UNSUBSCRIBE, CONNECT and DISCONNECT never count.
- A partly written packet continues through `_bytesSent` on the next call.
- The cap covers both callers of `_checkOutbox()`: the connected state (`:271`) and `disconnectingMqtt1` (`:293`).

**Provisional parts.** Both values, and whether the cap is needed at all, are settled by a bench A/B on .61 and .63 (TASK-2949.3) on four variants:

1. today;
2. Nagle only;
3. cap only (4 and 8);
4. both.

If Nagle alone keeps the largest one-`loop()` drop under 10 KB at the SAT takeover, at republish-all and at a boot discovery burst, the cap is still added to the fork but left at 0 in the firmware (YAGNI). If Nagle shows a latency problem (Open Question 3), the cap becomes the primary measure at `n = 4`.

```cpp
// fork Transport/ClientSync.cpp, in both connect() overloads (ESP32 branch)
#if EMC_TCP_NODELAY
    int val = true;
    client.setSocketOption(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
#endif

// fork MqttClient.cpp
void MqttClient::_checkOutbox() {
  uint16_t sent = 0;
  while (_sendPacket() > 0) {
    OutgoingPacket* p = _outbox.getCurrent();
    bool donePublish = p && _bytesSent == p->packet.size() &&
                       p->packet.packetType() == PacketType.PUBLISH;
    if (!_advanceOutbox()) break;
    if (donePublish && _maxPacketsPerLoop && ++sent >= _maxPacketsPerLoop) break;
  }
}
```

## Decision Contract

### Must

- Both knobs default to upstream behaviour (`EMC_TCP_NODELAY 1`, `EMC_MAX_PACKETS_PER_LOOP 0`), so the fork commits can go upstream unchanged.
- The firmware sets its values in one place (`platformio.ini` build flags or the MQTT setup in `MQTTstuff.ino`), with a comment citing this ADR and the bench numbers.
- Only PUBLISH packets count towards the cap.
- **Fault 5, the outbox grows under the cap.** The firmware never queues PUBLISH faster than the cap drains. Deferrable producers are held by the ADR-265 outbox gate. The event-driven producers stay at about 1 OT frame per second and at most 9 packets per fan-out. A 60 min soak reports the maximum outbox depth and shows no rise in refused publishes: `mqttPublishRaw()` false (`MQTTstuff.ino:344`), and espMqttClient refuses packets while the largest free block is below `EMC_MIN_FREE_MEMORY` 16,384 B (`Config.h:32`).
- **Fault 6, control packets starve behind the cap.** A control packet queued behind `k` PUBLISH packets leaves within `ceil(k / n) + 1` `loop()` calls, well inside the 60 s keepalive (`MQTTstuff.ino:1612`).
- Each change is a separate fork commit, marked with a comment naming this ADR (ADR-264 process).

### Must Not

- Must not drop or reorder packets. The outbox stays first-in-first-out.
- Must not block the loop task: the non-blocking write of ADR-186 stays.

### Exceptions

- None.

### Verification

- A host test compiles the real `MqttClient.cpp` with a stub transport:
  - 40 PUBLISH: `n = 0` sends 40 in one `loop()`, `n = 8` sends 8. Red on the code before the cap.
  - Fault 6: a PINGREQ and a PUBACK queued behind 40 PUBLISH at `n = 8` leave within 6 calls.
  - A half-accepted packet continues on the next call and is counted once.
- A host test checks that `ClientSync::connect()` sets `TCP_NODELAY` only when `EMC_TCP_NODELAY` is 1: the stub records the socket option.
- Bench A/B (TASK-2949.5) on .61 and .63, at three events: the TASK-2019 takeover, a republish-all and a boot discovery burst.
  - Measured: the largest drop inside one `loop()` (probe `at=13 from=12`), SAT publish latency, and the latency of each small publish.
  - The small-publish latency is measured explicitly, because Nagle plus the broker's delayed ACK can add about 40-200 ms per small publish (coordinator, 2026-11-20). Two measurements:
    - the command echo: a Home Assistant `set/` command to the gateway, until its state comes back on the broker;
    - the keepalive: PINGREQ to PINGRESP.
  - Pass: under 10 KB per `loop()` with the chosen variant, a command echo under 250 ms at p95, and no PINGRESP or keepalive timeout over the soak.

## Alternatives Considered

- **A. Per-loop PUBLISH cap alone.** It bounds about n x 1.5 KB per pass, for every packet type. Small packets still pay one segment each, so 4 SAT topics cost as much as 4 discovery configs. Simple and well-tested. Kept as the backstop, not the primary measure.
- **B. Coalescing in the library through a staging buffer.** Copy several packets into one buffer of about one MSS and issue one `send()`. It has the same effect as Nagle, without the ACK wait, but it is new code on the hot path. Partial writes then span packet boundaries, and the buffer needs about 1.4 KB more RAM. `EMC_TX_BUFFER_SIZE` (1,440) exists, but it only serves chunked payload callbacks (`Packets/Packet.cpp:430`, `:438`), not coalescing. Not chosen: Nagle gets the same segment saving from code lwIP already has.
- **B2. `MSG_MORE` on every `send()` except the last of a pass.** With `TF_NODELAY`, lwIP still pushes each write out at once, so the merge into the unsent segment probably does not happen. This is not verified in Espressif's lwIP. Not chosen.
- **C. Do nothing in the library; ADR-265 pacer only.** The event-driven publishers stay unbounded and the 16-segment worst case remains. Not chosen.
- **Earlier objection to Nagle (ADR-265 and analysis, first version).** That version rejected Nagle on latency, before the cost per segment was known. Nagle delays the tail of a burst by at most one ACK round trip. With a LAN broker that is a few milliseconds. A delayed ACK on the broker's host can stretch this to about 40 ms (Linux) to 200 ms. For a heating gateway with a 1 s OpenTherm cadence that is acceptable, if Open Question 3 shows no stalls. Re-weighed here.

## Consequences

**Positive:**

- The cost is attacked at its cause: fewer segments, so less transient heap, fewer pbufs and fewer ACKs.
- The change is one build flag in the firmware, plus two small, opt-in fork commits that can go upstream.
- The cap bounds the remaining large-packet bursts for every publisher, including the event-driven ones.

**Negative:**

- Nagle can delay command replies and status updates by up to one delayed-ACK interval. *Mitigation:* the bench measures round-trip time (Open Question 3). If it fails, the fallback is the cap at `n = 4` with Nagle off.
- A deep outbox drains more slowly under the cap. *Mitigation:* the ADR-265 pacer keeps deferrable producers from filling it. The soak checks fault 5.
- The 4-8 KB that the segment count does not explain stays unexplained for now (Open Question 2).

## Open Questions

- [ ] 1. The bench A/B picks the variant and the values: Nagle only, cap only (4 or 8), or both.
- [ ] 2. Where do the extra 4-8 KB go in runs 2#2 and 4? The candidates are tcp_seg/MEMP, RX pbufs, and a concurrent async_tcp allocation by the 2 s info poll. This is a separate measurement; it does not block this ADR.
- [ ] 3. With Nagle on, are command round-trip and SAT publish latency still acceptable: under 250 ms, and with no keepalive or PUBACK stall?
- [ ] 4. The upstream PR text for both knobs, after the bench, checked by the coordinator and Robert before it opens.
- [ ] 5. Acceptance by the maintainer.

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
