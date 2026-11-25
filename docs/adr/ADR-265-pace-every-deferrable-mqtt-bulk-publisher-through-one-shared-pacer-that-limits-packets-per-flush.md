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

**Amended in place on 2026-11-25 (still Proposed)**, after ADR-266 was Accepted as Nagle only, without the per-loop PUBLISH cap. Robert decided on 2026-11-25 that this pacer is built for 2.0.0 in four cuts before the stable, each behind its own compile-time switch, with Nagle (ADR-266) in its own soak. The library-cap reliance is removed, and the Decision Contract is split into a contract per cut and the target after cut 4.

**Amended in place again on 2026-11-25 (still Proposed)**, after Robert set the test scheme (2026-11-25, about 22:00, relayed by the coordinator): 15 min bench per task, 60 min on a public beta release build, and a 24 h bench run only for the final release, on the Classic + S3 Mini, with no field-unit count (Robert, about 22:20). One cause per build stays the attribution default for MQTT, TCP and heap changes, so Nagle and each cut ride their own beta. Each cut's switch gains a RAM-only runtime flag in prerelease builds, through the debug-flag table of ADR-206, so an anomaly is bisected by a toggle instead of a rebuild. Each paced producer gets two counters. ADR-206 itself is not changed.

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

The pacer bounds how much our deferrable producers have waiting in the outbox, and so how many of their packets one `MQTTclient.loop()` can still write. It does not bound the event-driven publishers, and it does not bound what already sits in lwIP: a QoS 0 packet leaves the outbox as soon as it is written (`espMqttClient/src/Packets/Packet.cpp:60-61`, `MqttClient.cpp:387-388`). The segment bound for every publisher is Nagle on the MQTT socket (ADR-266, Accepted 2026-11-25, Nagle only; the per-loop PUBLISH cap of its first version is dropped). The pacer keeps no per-pass counter of its own.

**Precedent.** The TASK-2019 pace window, the same outbox-and-block hold limited to SAT at a takeover, was benched not green on .61 (commit 54bd8b8bc, beta.403, run 4: -29.4 KB). Each cut therefore has its own bench criterion and ships OFF when it fails.

A per-producer liveness bound guarantees progress: after waiting `MQTT_PACE_MAX_WAIT_MS`, the producer publishes anyway.

The pacer counts packets, not bytes, because the best evidence so far (beta.403 runs 2 and 3, about 1.5 KB per packet regardless of payload) puts the flush cost on the packet count. Open Question 1 confirms this. The constants live in one place, and the existing per-gate thresholds are replaced by them.

Event-driven publishers stay outside the pacer. ADR-088 stays in force unchanged for them, and the discovery drip keeps consulting `isDripDeferred()`.

This is publish plumbing, not SAT control behaviour. It changes when a SAT topic leaves, never what SAT sends to the boiler. The SAT ground-truth rules (ADR-196) therefore do not apply; TASK-1757 is the precedent.

Example of the intended shape (illustrative; the implementation task sets the final constants from the Open Question 1 measurement):

```cpp
// MQTTstuff.ino, loop task only
// The per-pass segment bound is Nagle on the MQTT socket (ADR-266), not a library cap.
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

### Must (contract per cut; each cut ships with its own switch: a compile-time default, `false` = the code before that cut, plus a runtime flag in prerelease builds)

- **Cut 1, SAT shadow gate:** `satOutboxFull()` (`SATmqttPublish.cpp`) defers when `mqttPaceAllows()` says no, on top of its existing conditions, and keeps the shadow and `s_satPublishDeferred`. A liveness bound publishes one SAT helper per pass after `MQTT_PACE_MAX_WAIT_MS` without a yes. The post-connect pace window and the SAT limit of 24 stay until the target.
- **Cut 2, discovery drip:** `loopMQTTDiscovery()` asks `mqttPaceAllows()` before a drip publish, keeps its pending position, and keeps consulting `isDripDeferred()` (ADR-088).
- **Cut 3, connect stages:** `connStageTick()` asks `mqttPaceAllows()` for each portion, with the existing 5 s publish-anyway as its liveness. The decision on `publishAllPICsettings()` (Open Question 3) is made in this cut.
- **Cut 4, stale override sweep and hourly stats:** `staleOverrideClearTick()` and `loopMQTTStatsPacer()` ask `mqttPaceAllows()` and keep their pending state.
- **Every cut:** `MQTT_PACE_MAX_OUTBOX`, `MQTT_PACE_MIN_BLOCK` and `MQTT_PACE_MAX_WAIT_MS` are defined once, next to `mqttPaceAllows()`, with values from the Open Question 1 measurement; every paced producer has a liveness bound; the pacer runs on the loop task only, and a request from another task sets a flag the loop task consumes, as `queueMQTTRepublishAll()` does (`OTGW-Core.ino:2217`).
- **Every cut, runtime flag:** one `bool` per cut in `DebugSection` (`state.debug`), default `true`, with one row in `kDebugFlags` (`debugFlags.h`, ADR-206), so the telnet key and `GET/POST /api/v2/debug/flags` share it. The flag and its row exist only under `#if defined(_VERSION_PRERELEASE)`, like the bench hooks in `handleDebug.ino`; a stable build has the compile-time default only. The flag is RAM-only: no settings key, no persistence, a reboot sets it back to `true`. The pacer reads it on the loop task; a REST write from another task is a single `bool` store, as for the other debug flags. With the flag `false` the cut's producer behaves as the code before that cut.
- **Every cut, counters:** per paced producer, a count of "pace said no" and a count of "liveness fired", visible on telnet and in the `[HEAP]` telemetry, so an anomaly can be attributed to a producer without a bisect run.

### Must (target, after cut 4)

- Every deferrable bulk producer (the SAT shadow helpers, `loopMQTTDiscovery()`, `connStageTick()`, `staleOverrideClearTick()`, `publishAllPICsettings()` unless Open Question 3 decides otherwise, `loopMQTTStatsPacer()`) is paced only by `mqttPaceAllows()`. The per-gate constants, the post-connect window and `SAT_MQTT_OUTBOX_LIMIT` are removed; the compile-time switches and the runtime flags go once each cut has passed its gates and the final release's long run.

### Must Not

- Must not hold or drop a per-frame `processOT()` publish or a status fan-out. Those stay under ADR-088.
- Must not patch the vendored espMqttClient beyond what ADR-186, ADR-264 and ADR-266 allow. The pacer itself needs no library change.
- Must not add a settings key, a persisted value or a stable-build REST control for a cut's switch.
- Must not bring back heap-tier gating. ADR-167 stays: the pacer reads the largest free block as a send condition, not as a tier.
- Before the target: must not add a new outbox-depth threshold outside `mqttPaceAllows()`; the existing ones of gates not yet converted stay until their cut.
- After the target: must not compare `MQTTclient.queueSize()` or `mqttOutboxDepth()` against a threshold outside the pacer.

### Exceptions

- None.

### Verification

- A host test compiles the real `mqttPaceAllows()` and the real `SATmqttPublish.cpp`, then drives a first-seen SAT set of at least 140 topics. It counts the packets queued per pass. It must go red on the code before this ADR (up to 24 or 25 per pass) and green on the pacer (the outbox never holds more than `MQTT_PACE_MAX_OUTBOX` plus the event-driven packets of that pass). A second case keeps the outbox stuck and shows the liveness bound publishes everything.
- Per cut, a host test runs red on the code before the cut and green on it, with its switch `false` equal to the old behaviour, and mutants (pacer always yes, liveness removed, liveness publishing the whole set, runtime flag ignored) that turn a test red. One case toggles the runtime flag in the middle of a pass. `tests/test_debug_flags_api.py` stays green with the new rows.
- Until the target, a narrow `evaluate.py` check: each converted producer calls `mqttPaceAllows()`, and the `MQTT_PACE_*` constants are defined once. After cut 4, the full gate: it fails when `queueSize()` or `mqttOutboxDepth()` is compared against a number outside `mqttPaceAllows()` (TASK-2949.10).
- Bench: the TASK-2019 takeover recipe and the TASK-1757 W/C/B boot recipe on .61. No `[HEAP] sat-takeover-probe` or connect-probe drop of 10 KB or more at `at=13 from=12`. Old-vs-new numbers go in the task. A/B runs toggle the runtime flag on one binary (same heap layout).
- Per cut on .61, on the cut's build: a 15 min bench with forced events, each once: a broker outage of about 30 s (connect stages only) and one of more than 300 s (`MQTT_REPUBLISH_OFFLINE_THRESHOLD_MS`, the only trigger of the value republish and discovery reset after a reconnect), republish-all, discovery republish, SAT disable and enable, and a WiFi drop (recipe `OTGW-field-data/_comms/soak12_recipe.md`). Then 60 min on the public beta release build that carries the cut. The final release runs 24 h on the bench (Classic + S3 Mini; a parallel run on a board without PSRAM is recommended, because the heap margin shows only there) on the production binary itself: the `release/2.x` build with `_VERSION_PRERELEASE` off, which has no runtime flags. The file that ran is the file that is published (same SHA-256).
- Per cut, a host test compiles the cut's code with `_VERSION_PRERELEASE` defined and undefined, and shows that the build without the flag behaves as the build with the flag ON.

## Alternatives Considered

- **Keep the patchwork and add triggers as they come (the TASK-2019 fix).** As a structure it fails open: every new bulk trigger needs someone to remember to open the window, and six thresholds on one resource cannot be reasoned about together. Its pace window was also benched not green at the takeover (TASK-2019, beta.403 run 4). Rejected; the parked branch is not landed, and cut 1 replaces it.
- **Extend ADR-088's burst window to every publisher.** The ADR-088 window only makes the discovery drip wait. It does not slow the publisher inside the window, so it would not have prevented either measured dip. Widening it would also change a binding, gated rule for no gain. Rejected.
- **Turn Nagle back on for the MQTT socket, as the only measure.** Accepted separately as ADR-266 (2026-11-25), through the fork (ADR-264), in its own soak. It bounds the segments of every publisher, but not how much our deferrable producers queue in one pass, which is what this pacer bounds. Both are kept: they act on different quantities.
- **Limit packets per `loop()` inside espMqttClient, as the only measure.** Robert asked for it on 2026-11-20 and dropped it on 2026-11-25 (ADR-266, Open Question 1): with Nagle the broker socket's send queue peaked at 2-3 segments, so the cap had no measured failure to bound. Not chosen.
- **Cap the MQTT socket's lwIP send credit, as ADR-216 does for port 80.** `snd_buf` limits bytes, not segments. With Nagle off, 16 tiny segments still fit in a 2,872 B credit. On the measured cost, this would not bound the dip. Rejected.
- **Make the gate byte-based.** The measured cost is about ten times the payload and follows the packet count (about 1.5 KB per packet in beta.403 runs 2 and 3), so bytes are the wrong unit. Rejected, pending Open Question 1.

## Consequences

**Positive:**

- One rule and one place to tune. A new bulk trigger (a takeover, an enable, a republish, a future feature) is paced without anyone remembering to open a window.
- The connect window, the TASK-2019 trigger flags and five sets of constants go away.
- One host test plus one `evaluate.py` gate guards the whole class, instead of per-trigger regressions.

**Negative:**

- An always-on pacer slows steady-state SAT publishing and discovery a little. A change waits up to one loop pass, and up to `MQTT_PACE_MAX_WAIT_MS` under load. Mitigation: SAT already retries every 500 ms. The spike measures the steady-state outbox depth (the comment at `SATmqttPublish.h:89` says 0 to 35 packets on the bench) before the constants are fixed.
- Per-frame and status publishes are not held back by the pacer. Their segments are bounded by Nagle once ADR-266 is built, and ADR-088 keeps the drip out of their way. Until Nagle is built, a cut may not reach its bench criterion (the TASK-2019 precedent above).
- A large remaining drop is not an MQTT publish at all: `satControlLoopRun` (16-20 KB in ADR-266's table, TASK-3005). No cut of this pacer touches it.
- The cause of the per-packet cost stays unattributed until the spike runs. If it turns out to be something else, the unit (packets) may need revisiting. Mitigation: Open Question 1 blocks acceptance.

## Open Questions

- [ ] 1. What does one queued and flushed MQTT packet cost on .61 on the build that ships? Partly answered: TASK-2949.5 measured `TCP_NODELAY` against Nagle (ADR-266's table), and TASK-2019 measured about 1.3-1.5 KB per packet per loop with `TCP_NODELAY` (16 packets, 20-24 KB). Still open: the cost per packet in the SAT publish pass itself, which sets `MQTT_PACE_MAX_OUTBOX` (step 0 of the implementation plan, `OTGW-field-data/_comms/adr265_plan.md`). The ADR-266 cap value is no longer needed.
- [ ] 2. Steady-state impact: with the pacer on, what is the added SAT publish latency and the discovery completion time after boot, compared with today?
- [ ] 3. Should `publishAllPICsettings()` (up to 15 publishes on a Classic) move under the pacer, or stay inside connect stage 3 as one portion?
- [ ] 4. Acceptance by the maintainer.

## Related Decisions

- ADR-088 (MQTT status-burst windowing and cooldown): stays in force for status fan-outs and the drip. Related, not superseded.
- ADR-131 (espMqttClient engine, single publish chokepoint): the loop-task pumping model this pacer depends on.
- ADR-186 (vendored espMqttClient, non-blocking write) and ADR-187 (track upstream): why the pacer sits on the producer side, not in the library.
- ADR-167 (heap tier gating retired): the pacer is not a tier gate.
- ADR-266 (Accepted 2026-11-25: Nagle on the MQTT socket, via the fork; the per-loop cap dropped): the segment bound for every publisher. The pacer does not depend on it, but a cut's bench result does.
- ADR-264 (Accepted: espMqttClient from our fork as a vendored copy of a pinned tag): the route for the Nagle change.
- ADR-216 (web send-queue cap): the transport-level precedent, and why it does not carry over to small MQTT packets.
- ADR-206 (Accepted: debug flags by name from one shared table): the cut switches use its table as prerelease-only rows. Not amended; this ADR records that use.
- ADR-051 (settings and state): the runtime flag lives in the state struct (`DebugSection`), not in settings.
- ADR-090 (re-entrancy guards): the per-pass counter is loop-task state; `doBackgroundTasks()` re-entry must not reset it mid-pass.

## References

- TASK-1757 (connect burst, probes, beta.218 fix), TASK-1339 (SAT outbox gate), TASK-2019 (takeover dip, in progress), TASK-1991 (stale override sweep).
- `src/OTGW-firmware/MQTTstuff.ino:344` (`mqttPublishRaw`), `:1571` (`MQTTclient.loop()`), `:1843-1846`, `:1894-1901`, `:1914-1921`, `:2808-2814`.
- `src/OTGW-firmware/SATmqttPublish.cpp:50-55`, `src/OTGW-firmware/SATmqttPublish.h:89`.
- `src/libraries/espMqttClient/src/MqttClient.cpp:354` (`_checkOutbox`), `Transport/ClientSync.cpp:32`, `:50`, `:60-74`.
- `D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3` sdkconfig: `CONFIG_LWIP_TCP_SND_BUF_DEFAULT 5744`, `CONFIG_LWIP_TCP_MSS 1436`, `CONFIG_LWIP_TCP_OVERSIZE_MSS 1`, `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM 32`; `include/lwip/lwip/src/include/lwip/opt.h:1400` (`TCP_SND_QUEUELEN`).
- Analysis: `docs/plans/2026-11-20-mqtt-burst-pacing-analysis.md`.
