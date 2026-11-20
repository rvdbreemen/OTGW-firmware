---
id: "ADR-265"
title: "Pace every deferrable MQTT bulk publisher through one shared pacer that limits packets per flush"
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
  - "heap"
  - "backpressure"
  - "burst-pacing"
aliases:
  - "MQTT pacer"
  - "outbox pacing"
  - "MQTT burst pacing"
  - "publish admission gate"
components:
  - "mqttPaceAllows (MQTTstuff.ino)"
  - "MQTT bulk-publisher pacer"
symbols:
  - "mqttPaceAllows"
  - "mqttConnectPaceHold"
  - "satOutboxFull"
  - "dripOutboxWait"
  - "connStageReady"
  - "staleOverrideClearTick"
  - "loopMQTTStatsPacer"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-265 Pace every deferrable MQTT bulk publisher through one shared pacer that limits packets per flush

## Status

Proposed, 2026-11-20.

**Decision Maker:** Agent: Claude (otgw-architect), on a design question from the coordinator. Acceptance is Robert's.

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

**The problem in one sentence.** When our code queues many MQTT (Message Queuing Telemetry Transport) packets in one loop pass, the next `MQTTclient.loop()` sends them all at once, and free heap falls 20 to 43 KB for a moment. On an ESP32-S3 (Espressif dual-core microcontroller) without PSRAM (pseudo-static RAM), that is enough to reach the CRITICAL tier (3,072 B) when a browser loads the web UI at the same time.

**Why the whole outbox leaves at once.** The firmware pumps espMqttClient from the loop task (`UseInternalTask::NO`, `MQTTstuff.ino:217`, ADR-131). One `MQTTclient.loop()` call (`MQTTstuff.ino:1571`) runs `_checkOutbox()` (`src/libraries/espMqttClient/src/MqttClient.cpp:354`): `while (_sendPacket() > 0)` keeps writing packets until the socket refuses. The ADR-186 patch makes that write non-blocking (`Transport/ClientSync.cpp:60-74`), but it does not limit how many packets go out per call.

**Measured cost (two independent sources):**

- TASK-1757 probes (beta.215, 12 boots): the largest drop is always inside one `MQTTclient.loop()` call, 23 to 43 KB, right after the SAT (Smart Autotune Thermostat) always-on set grew the outbox from 4 to 18 packets. Queueing those packets cost only about 5 KB. Fixed in beta.218 by the post-connect pace window.
- TASK-2019 (beta.369 to 375): the same drop at a SAT takeover after "outside temperature valid again", 20 to 38 KB inside one `MQTTclient.loop()`. otgw-dev's host model of the real `SATmqttPublish.cpp` (`tests/test_sat_takeover_outbox_burst.py` on lane-2019t, commit 365830e79) shows the SAT pass queues at most about 25 packets of about 2.8 KB payload in total.

So the flush costs about ten times the payload bytes.

**Contested premise (beta.403, .61, 2026-11-20, otgw-bench-tester via the coordinator):** at a SAT takeover, one probe at `at=13 from=12` still fell 29.4 KB with outbox 9 and mqttTx 749 B (maxblk 7,668 B). That is about 3.3 KB per packet for packets of about 83 B. It does not show a clean per-packet scale, and something else in `loop()` may hold the bytes. otgw-dev's beta.403 runs 2 and 3 point the other way: outbox 25 to 9, so 16 packets handed to lwIP in one `loop()`, cost 20.3 and 23.9 KB, about 1.5 KB per packet regardless of payload, and the heap recovered within 14 ms. That fits one MSS-sized segment per packet, at most `TCP_SND_QUEUELEN` 16 per flush. The two readings conflict; Open Question 1 (TASK-2949.1) settles them before any pacing code is written.

**Where the extra bytes go: not established.** The lwIP (lightweight IP stack) configuration in the pinned framework is as follows (`framework-arduinoespressif32-libs/esp32s3`, sdkconfig and `lwipopts.h:627`):

- `TCP_SND_BUF` = 5,744 B
- `TCP_MSS` = 1,436
- `TCP_SND_QUEUELEN` = 16 segments
- `TCP_OVERSIZE` = MSS
- WiFi dynamic TX buffers: 32

The MQTT socket sets `TCP_NODELAY` (`ClientSync.cpp:32`, `:50`). With Nagle off, every small packet becomes its own segment with its own pbuf and WiFi TX buffer. That points to a per-segment cost, but it has not been measured per allocator. Open Question 1 asks for that measurement.

**Today's patchwork: seven gates on one resource.** Each gate was added when a new trigger showed up:

| Gate | Where | Outbox limit | Block limit | Liveness |
|---|---|---|---|---|
| ADR-088 status-burst window and cooldown | `MQTTstuff.ino:135-201` | none | none | 500 ms timeout |
| Connect stages (TASK-1757) | `MQTTstuff.ino:1843-1846` | 2 | 20,480 B | 200 ms gap, then 5 s publish-anyway |
| Post-connect pace window (TASK-1757) | `MQTTstuff.ino:1914-1921` | 2 | 20,480 B | opens only at connect, 60 s hard end |
| Stale override sweep (TASK-1991) | `MQTTstuff.ino:1894-1901` | 2 | none | 100 ms gap |
| Discovery drip (TASK-1757 B) | `MQTTstuff.ino:2808-2814` | 8 | none | 5 skipped ticks, then publish |
| SAT shadow gate (TASK-1339) | `SATmqttPublish.cpp:50-55`, `SATmqttPublish.h:89` | 24 | none, except inside the pace window | retried every 500 ms |
| Hourly stats pacer (TASK-2004) | `MQTTstuff.ino:2380-2434` | none | waits on HEAP_CRITICAL | one stat per 100 ms |

The TASK-2019 fix for 2.0.0 opens the post-connect window at more triggers: SAT takeover, SAT enable and republish-all. Each new trigger needs its own call site. A trigger that someone forgets brings the dip back.

**Which publishers can wait and which cannot.** Only a publisher that keeps a shadow value or a pending bit can be held back and retried without losing data:

| Can wait (shadow or pending bit) | Cannot wait (event-driven) |
|---|---|
| SAT shadow helpers | Per-frame `processOT()` publishes |
| Discovery drip | Status fan-outs |
| Connect stages | |
| Stale override sweep | |
| PIC (Microchip microcontroller) settings | |
| Hourly stats (TASK-2004) | |

The event-driven publishers are naturally paced by the OpenTherm bus, at about one frame per second. Their fan-outs are at most 9 packets.

## Decision

Every deferrable MQTT bulk publisher asks one shared pacer, `mqttPaceAllows()` in `MQTTstuff.ino`, before it publishes. The pacer is always on, not opened per trigger.

It allows a publish only while both of these hold:

- the outbox holds at most `MQTT_PACE_MAX_OUTBOX` packets;
- the largest free block is at least `MQTT_PACE_MIN_BLOCK` bytes.

The segments that one `MQTTclient.loop()` hands to lwIP are bounded in the library (ADR-266: Nagle on for the MQTT socket, with a per-loop PUBLISH cap as backstop). That is the per-pass bound, and it covers the event-driven publishers too. The pacer bounds how much waits in the outbox; the cap bounds how much leaves per pass. The pacer therefore keeps no per-pass counter of its own.

A per-producer liveness bound guarantees progress: after waiting `MQTT_PACE_MAX_WAIT_MS`, the producer publishes anyway.

The pacer counts packets, not bytes, because the best evidence so far (beta.403 runs 2 and 3, about 1.5 KB per packet regardless of payload) puts the flush cost on the packet count. Open Question 1 confirms this. The constants live in one place, and the existing per-gate thresholds are replaced by them.

Event-driven publishers stay outside the pacer. ADR-088 stays in force unchanged for them, and the discovery drip keeps consulting `isDripDeferred()`.

This is publish plumbing, not SAT control behaviour. It changes when a SAT topic leaves, never what SAT sends to the boiler. The SAT ground-truth rules (ADR-196) therefore do not apply; TASK-1757 is the precedent.

Example of the intended shape (illustrative; the implementation task sets the final constants from the Open Question 1 measurement):

```cpp
// MQTTstuff.ino, loop task only
// The per-loop flush bound is the library cap (ADR-266): MQTTclient.setMaxPacketsPerLoop(MQTT_MAX_PACKETS_PER_LOOP)
bool mqttPaceAllows() {
  if ((uint32_t)MQTTclient.queueSize() > MQTT_PACE_MAX_OUTBOX) return false;
  if (platformMaxFreeBlock() < MQTT_PACE_MIN_BLOCK) return false;
  return true;
}

// SATmqttPublish.cpp
static inline bool satOutboxFull() {
  if (mqttPaceAllows()) return false;
  s_satPublishDeferred = true;                  // shadow untouched, retried next pass
  return true;
}
```

## Decision Contract

### Must

- Every deferrable bulk producer calls `mqttPaceAllows()` before a publish and keeps its shadow or pending bit when the answer is no:
  - SAT shadow helpers in `SATmqttPublish.cpp`
  - `loopMQTTDiscovery()`
  - `connStageTick()`
  - `staleOverrideClearTick()`
  - `publishAllPICsettings()`
  - `loopMQTTStatsPacer()`
- `MQTT_PACE_MAX_OUTBOX`, `MQTT_PACE_MIN_BLOCK` and `MQTT_PACE_MAX_WAIT_MS` are defined once, next to `mqttPaceAllows()`. The library cap value `MQTT_MAX_PACKETS_PER_LOOP` (ADR-266) is set in the same place.
- Every paced producer has a liveness bound: after `MQTT_PACE_MAX_WAIT_MS` without a yes, it publishes anyway.
- The pacer runs on the loop task only, like espMqttClient itself. A request from another task sets a flag that the loop task consumes, as `queueMQTTRepublishAll()` does (`OTGW-Core.ino:2217`).

### Must Not

- Must not hold or drop a per-frame `processOT()` publish or a status fan-out. Those stay under ADR-088.
- Must not compare `MQTTclient.queueSize()` or `mqttOutboxDepth()` against a threshold outside the pacer.
- Must not patch the vendored espMqttClient beyond what ADR-186 allows.
- Must not bring back heap-tier gating. ADR-167 stays: the pacer reads the largest free block as a send condition, not as a tier.

### Exceptions

- None.

### Verification

- A host test compiles the real `mqttPaceAllows()` and the real `SATmqttPublish.cpp`, then drives a first-seen SAT set of at least 140 topics. It counts the packets queued per pass. It must go red on the code before this ADR (up to 24 or 25 per pass) and green on the pacer (the outbox never holds more than `MQTT_PACE_MAX_OUTBOX` plus the event-driven packets of that pass). A second case keeps the outbox stuck and shows the liveness bound publishes everything.
- An `evaluate.py` gate fails when `queueSize()` or `mqttOutboxDepth()` is compared against a number outside `mqttPaceAllows()`.
- Bench: the TASK-2019 takeover recipe and the TASK-1757 W/C/B boot recipe on .61. No `[HEAP] sat-takeover-probe` or connect-probe drop of 10 KB or more at `at=13 from=12`. Old-vs-new numbers go in the task.

## Alternatives Considered

- **Keep the patchwork and add triggers as they come (the TASK-2019 2.0.0 fix).** This is the right minimal fix for the 2.0.0 stable release. As a structure it fails open: every new bulk trigger needs someone to remember to open the window. Six thresholds on one resource cannot be reasoned about together. Rejected for 2.1.0; kept as the baseline this ADR replaces.
- **Extend ADR-088's burst window to every publisher.** The ADR-088 window only makes the discovery drip wait. It does not slow the publisher inside the window, so it would not have prevented either measured dip. Widening it would also change a binding, gated rule for no gain. Rejected.
- **Turn Nagle back on for the MQTT socket.** The debug telnet socket already does this (`networkStuff.ino:1285`). This acts at the suspected cause: small writes would coalesce into full segments. But `setNoDelay(true)` sits in the vendored `ClientSync.cpp:32` and `:50`, and ADR-186 allows changes only in `ClientSync::write()`. Nagle would also add up to one RTT (round-trip time) of latency, plus the delayed-ACK interaction, to every command reply and status publish. This needs its own superseding ADR and is worth measuring in the Open Question 1 spike, not choosing blind. Not chosen now.
- **Limit packets per `loop()` inside espMqttClient, as the only measure.** This bounds the flush for every producer, including the event-driven ones. Robert decided on 2026-11-20 to carry it in our fork (fork as source: ADR-264; the cap itself: ADR-266, conditional on the TASK-2949.1 measurement). If accepted, it is the per-pass bound, not the only measure. On its own it lets the outbox grow without limit when deferrable producers queue faster than the cap drains (ADR-266, fault 5). The pacer's outbox bound closes that gap.
- **Cap the MQTT socket's lwIP send credit, as ADR-216 does for port 80.** `snd_buf` limits bytes, not segments. With Nagle off, 16 tiny segments still fit in a 2,872 B credit. On the measured cost, this would not bound the dip. Rejected.
- **Make the gate byte-based.** The measured cost is about ten times the payload and follows the packet count (about 1.5 KB per packet in beta.403 runs 2 and 3), so bytes are the wrong unit. Rejected, pending Open Question 1.

## Consequences

**Positive:**

- One rule and one place to tune. A new bulk trigger (a takeover, an enable, a republish, a future feature) is paced without anyone remembering to open a window.
- The connect window, the TASK-2019 trigger flags and five sets of constants go away.
- One host test plus one `evaluate.py` gate guards the whole class, instead of per-trigger regressions.

**Negative:**

- An always-on pacer slows steady-state SAT publishing and discovery a little. A change waits up to one loop pass, and up to `MQTT_PACE_MAX_WAIT_MS` under load. Mitigation: SAT already retries every 500 ms. The spike measures the steady-state outbox depth (the comment at `SATmqttPublish.h:89` says 0 to 35 packets on the bench) before the constants are fixed.
- Per-frame and status publishes are not held back by the pacer. Their flush is bounded by the library cap (ADR-266), and ADR-088 keeps the drip out of their way.
- The cause of the per-packet cost stays unattributed until the spike runs. If it turns out to be something else, the unit (packets) may need revisiting. Mitigation: Open Question 1 blocks acceptance.

## Open Questions

- [ ] 1. What does one flushed MQTT packet cost on .61, and where does it go (lwIP pbufs and segments, WiFi dynamic TX buffers, other)? Measure KB per packet for 4, 8, 16 and 24 packets in one flush, with `TCP_NODELAY` on and off. This sets the ADR-266 cap value and confirms packets as the unit (this is also ADR-266's precondition).
- [ ] 2. Steady-state impact: with the pacer on, what is the added SAT publish latency and the discovery completion time after boot, compared with today?
- [ ] 3. Should `publishAllPICsettings()` (up to 15 publishes on a Classic) move under the pacer, or stay inside connect stage 3 as one portion?
- [ ] 4. Acceptance by the maintainer.

## Related Decisions

- ADR-088 (MQTT status-burst windowing and cooldown): stays in force for status fan-outs and the drip. Related, not superseded.
- ADR-131 (espMqttClient engine, single publish chokepoint): the loop-task pumping model this pacer depends on.
- ADR-186 (vendored espMqttClient, non-blocking write) and ADR-187 (track upstream): why the pacer sits on the producer side, not in the library.
- ADR-167 (heap tier gating retired): the pacer is not a tier gate.
- ADR-266 (per-loop PUBLISH cap in espMqttClient, Proposed and conditional on the TASK-2949.1 measurement): the library-side per-pass bound this pacer relies on once accepted; ADR-266 (espMqttClient from our fork) is where that cap would live.
- ADR-216 (web send-queue cap): the transport-level precedent, and why it does not carry over to small MQTT packets.
- ADR-090 (re-entrancy guards): the per-pass counter is loop-task state; `doBackgroundTasks()` re-entry must not reset it mid-pass.

## References

- TASK-1757 (connect burst, probes, beta.218 fix), TASK-1339 (SAT outbox gate), TASK-2019 (takeover dip, in progress), TASK-1991 (stale override sweep).
- `src/OTGW-firmware/MQTTstuff.ino:344` (`mqttPublishRaw`), `:1571` (`MQTTclient.loop()`), `:1843-1846`, `:1894-1901`, `:1914-1921`, `:2808-2814`.
- `src/OTGW-firmware/SATmqttPublish.cpp:50-55`, `src/OTGW-firmware/SATmqttPublish.h:89`.
- `src/libraries/espMqttClient/src/MqttClient.cpp:354` (`_checkOutbox`), `Transport/ClientSync.cpp:32`, `:50`, `:60-74`.
- `D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3` sdkconfig: `CONFIG_LWIP_TCP_SND_BUF_DEFAULT 5744`, `CONFIG_LWIP_TCP_MSS 1436`, `CONFIG_LWIP_TCP_OVERSIZE_MSS 1`, `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM 32`; `include/lwip/lwip/src/include/lwip/opt.h:1400` (`TCP_SND_QUEUELEN`).
- Analysis: `docs/plans/2026-11-20-mqtt-burst-pacing-analysis.md`.
