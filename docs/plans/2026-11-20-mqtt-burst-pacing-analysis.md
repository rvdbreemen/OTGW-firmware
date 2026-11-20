# MQTT burst pacing: analysis and recommendation (2.0.0)

Author: otgw-architect (Claude), 2026-11-20, on a design question from the coordinator.
Basis: origin/dev 1292d9948. The decision proposals are ADR-265 (pacer), ADR-264 (fork as source) and ADR-266 (segment bound in the library: Nagle on plus a per-loop cap as backstop), all Proposed. The epic is TASK-2949, milestone 2.0.0 (Robert, 2026-11-20).

## Update, 2026-11-20 afternoon: Robert's decisions

Robert's decisions, relayed by the coordinator:

1. Carry the change in our espMqttClient fork and make the fork the dependency from now on.
2. All of this goes into 2.0.0: the fork, the per-loop cap and the whole pacer consolidation.
3. The 2.0.0 stable release moves to Sunday 29 November.

Options D and E below were weighed against ADR-186 and ADR-187. After these decisions, option E (a per-`loop()` limit in espMqttClient) is adopted through the fork, as a segment bound in the fork (ADR-266, Proposed): Nagle on for the MQTT socket as the primary measure, a PUBLISH-only cap per `loop()` as backstop, settled by a bench A/B. The fork as dependency source is a separate decision, ADR-264.

The measurements are still being confirmed (TASK-2949.1). otgw-dev's beta.403 runs 2 and 3 show 16 packets per `loop()`, which is `TCP_SND_QUEUELEN`, costing 20-24 KB, about 1.5 KB per segment. Takeover 1 showed -29.4 KB at outbox 9. ADR-266 therefore weighs coalescing as an alternative to the cap. It becomes the pacer's per-pass bound, and the pacer (ADR-265) keeps only the outbox-depth and largest-block bounds plus liveness.

Execution order in epic TASK-2949, milestone 2.0.0:

1. Measure that the dip scales with the packets sent per `loop()`.
2. Add the fork changes (#191, TASK-1325, the cap) and tag the fork.
3. Switch the dependency to the fork tag, with a provenance file.
4. Use the cap in the firmware.
5. Build `mqttPaceAllows()`.
6. Migrate the gates one at a time.
7. Add the `evaluate.py` gate.

## Question

The coordinator asked three things:

1. Should all publishers share one generic burst pacer, or do we keep today's patchwork?
2. Does ADR-088 need a successor ADR or an amendment?
3. Which gate (ADR-080) belongs with it?

## Answer first

1. **One pacer, for every publisher that can wait.** That covers the SAT shadow helpers, the discovery drip, the connect stages, the stale override sweep, the hourly stats pacer (TASK-2004) and, to be decided, the PIC settings. The pacer is always on, so no call site has to open a window when a new trigger shows up. It counts packets per flush, not bytes. Publishers that cannot wait, the per-frame `processOT()` publishes and the status fan-outs, stay outside it.
2. **ADR-088 stays unchanged.** It is binding, gated and correct within its own scope: the drip yields to status fan-outs. The new rule is a separate ADR, ADR-265, that relates to ADR-088 and does not supersede it. Folding the two together later is not needed now (YAGNI).
3. **Gate:** a host test compiles the real pacer and the real `SATmqttPublish.cpp`. It goes red on today's 24-25 packets per pass and green on the pacer, and it also covers the liveness bound. An `evaluate.py` check that forbids outbox-depth thresholds outside the pacer supports it. The check prevents a seventh ad-hoc gate. It does not replace the test.

The TASK-2019 fix for 2.0.0 opens the existing pace window at the takeover, at SAT enable and at republish-all. It is the right minimal fix for the stable release, and the 2.1.0 epic replaces it. It does not block the epic, and the epic does not block it.

## Facts (verified in code or measurement)

| Fact | Source |
|---|---|
| A single `MQTTclient.loop()` sends the whole outbox: `while (_sendPacket() > 0)` until the socket refuses | `src/libraries/espMqttClient/src/MqttClient.cpp:354`; non-blocking write `Transport/ClientSync.cpp:60-74` (ADR-186) |
| espMqttClient is pumped from the loop task | `MQTTstuff.ino:217` (`UseInternalTask::NO`), `:1571` |
| Every publish goes through one chokepoint | `MQTTstuff.ino:344` (`mqttPublishRaw`, QoS 0) |
| The flush, not the queueing, costs the heap: 23-43 KB inside one `loop()`, queueing 14 packets cost about 5 KB | TASK-1757 notes, beta.215 probes, 12 boots |
| Same mechanism at the SAT takeover: 20-38 KB inside one `loop()` | TASK-2019 notes, clean takeover probes |
| A SAT pass queues at most about 25 packets and about 2.8 KB of payload | otgw-dev host model `tests/test_sat_takeover_outbox_burst.py` (lane-2019t, 365830e79) |
| MQTT socket has Nagle off | `ClientSync.cpp:32`, `:50` (`setNoDelay(true)`) |
| lwIP: `TCP_SND_BUF` 5,744, MSS 1,436, `TCP_SND_QUEUELEN` 16, `TCP_OVERSIZE` = MSS; WiFi dynamic TX buffers 32 | framework-arduinoespressif32-libs/esp32s3 sdkconfig, `lwipopts.h:627`, `opt.h:1400` |

## Inference (labelled, not proven)

The flush costs about ten times the payload bytes. Two sources, the probe deltas and the host model, put that number there. With Nagle off, every small packet becomes its own TCP segment, with its own pbuf and WiFi TX buffer. So the cost most likely scales with the number of segments, and the pacer should count packets. Which allocator takes the bytes has not been measured. The first subtask of the epic, a spike, settles that and sets the per-loop cap value (ADR-266).

Note on lwIP's oversize allocation: with `TF_NODELAY` set and no `MSG_MORE` flag, upstream lwIP 2.1 `tcp_pbuf_prealloc()` does not add the oversize margin. Per-segment lwIP memory may therefore be close to the payload, and the bulk could sit in the WiFi driver. This is read from upstream lwIP source, not checked against Espressif's fork. It is one more reason to measure before fixing the constants.

## The patchwork today

Seven gates guard one resource, the MQTT outbox flush. They use four different outbox limits (none, 2, 8 and 24), three different heap conditions (none, a 20,480 B largest block, and HEAP_CRITICAL in the TASK-2004 stats pacer) and six liveness rules. The table with file:line is in ADR-265, Context.

The structural weakness is that the window has to be opened per trigger:

- TASK-1757 opened it at connect.
- TASK-2019 adds the takeover, SAT enable and republish-all.

The next bulk trigger will bring the dip back until someone finds it on the bench.

## Options weighed

| Option | Bounds every bulk producer | Stays within ADR-186/187 | Steady-state cost | Verdict |
|---|---|---|---|---|
| A. Patchwork plus trigger flags (TASK-2019) | Only known triggers | yes | none | 2.0.0 only |
| B. Widen ADR-088's window | no, the window only makes the drip wait | yes | none | rejected |
| C. Always-on producer-side pacer (ADR-265) | yes, every deferrable producer | yes | a small publish delay, to be measured | **recommended** |
| D. Nagle on for the MQTT socket | yes, every producer including event-driven ones | no, needs a superseding ADR for the vendored `ClientSync` | up to 1 RTT per reply | measure in the spike, decide after |
| E. Per-`loop()` budget inside espMqttClient | yes | no, diverges from upstream | none | fallback only |
| F. `snd_buf` cap as in ADR-216 | no: it bounds bytes, not segments | yes | small | rejected |

**Why C wins.** It is the only option that covers every bulk trigger, including future ones, without touching the vendored library. It removes code: the connect window, the TASK-2019 flags and five sets of constants go away. One test and one check guard it.

**Main risk.** The steady-state latency and the discovery time after boot change. The spike measures both before the constants are fixed. Option D stays open as an add-on if the spike shows that the event-driven fan-outs alone still cause dips that matter.

## Scope boundaries

- SAT control behaviour does not change. The pacer only decides when a SAT topic leaves, never what goes to the boiler. The Sergeant D ground-truth rules therefore do not apply; TASK-1757 is the precedent.
- No heap-tier gating comes back (ADR-167). The largest-block check is a send condition, as in TASK-1757.
- Loop task only. Requests from other tasks keep using the flag pattern (`queueMQTTRepublishAll()`, `OTGW-Core.ino:2217`).

## Open-source check

There is nothing to adopt. The pacer is a predicate plus a per-pass counter inside our own publish path: trivial glue in the project's own code. The library-side alternatives, D and E, were weighed above against ADR-186 and ADR-187.
