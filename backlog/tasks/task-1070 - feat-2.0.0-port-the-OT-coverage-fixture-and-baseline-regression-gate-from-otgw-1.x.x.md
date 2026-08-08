---
id: TASK-1070
title: >-
  feat-2.0.0: port the OT coverage fixture and baseline regression gate from
  otgw-1.x.x
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 16:05'
updated_date: '2026-08-08 21:21'
labels:
  - test
  - tooling
dependencies: []
priority: high
ordinal: 257000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 1.x line has a decode-coverage fixture (423 frames, all 134 OTmap ids, all 6 message types, all 5 source prefixes), a normalized-fingerprint baseline gate and a one-command runner. That gate found two real defects within an hour of existing (TASK-1064 Remeha ids 131-133 never decoding, TASK-1066 four vh_* topics never heartbeating). Both defects were also present here and have now been ported blind as TASK-1068/1069, verified only by build and inspection, because 2.0.0 has no equivalent gate to confirm them on device. Porting the gate closes that hole and makes future ports verifiable. Compatibility already checked: 2.0.0 exposes /api/v2/simulate and /upload, its OTmap is identical to 1.x (134 entries, max id 133), and the 1.x parser was run unmodified against a real 2.0.0 capture (transcript-pro-alpha288) and correctly extracted 100 OT keys, 45 MsgIDs, 95 topics and all six message types. So the fixture and parser port as-is; only paths, the baseline recording and the expected-asset wiring are branch-specific.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/tests/ on this branch carries the coverage fixture, its generator, the baseline tool and the one-command runner
- [ ] #2 The generator regenerates the fixture from this branch's OTmap and the shared capture corpus, and the result is frames-only with no comment lines
- [ ] #3 The runner works against an ESP32-S3 bench device: upload, start, capture, stop, compare, and stops the simulation even on failure
- [ ] #4 A baseline is recorded from a steady-state multi-loop run on 2.0.0 hardware, not from a boot window
- [ ] #5 selftest reports 0 unstable keys on the recorded baseline capture
- [ ] #6 An independent run against the recorded baseline reports PASS
- [ ] #7 The gate is used to confirm TASK-1068 and TASK-1069 on device, closing their outstanding hardware ACs
- [ ] #8 scripts/tests/README or equivalent documents the workflow and the branch-specific bits
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-08 gate port to 2.0.0 (bench: Classic S3 + PIC 6.6, 192.168.88.63, alpha.354+a7e06f8).

WHAT THE FIRST 2-LOOP RUN PROVED. Capture clean (no reboot, monotonic 21:08-21:19, uptime counters steady). Live-PIC leakage ruled out: R00000000 appears exactly 2x, i.e. once per loop, so picSerialFlushRx does discard live RX during replay. settings.sat.bSimulation is false, so the SAT boiler sim is not a second async frame source either. Still FAILed with 2 diffs, and the cause is neither of those.

ROOT CAUSE: the debug telnet drops output under a publish burst, and the dropped bytes take a decode line with them. Capture line 2250 is the proof, one line carrying a truncation and the next timestamp spliced onto it:
  21:14:56.141905 ... sendMQTTData(1402): Sending MQTT: ... => To21:14:56.881179 (  43620| 31732) logMQTTValue(1678): MQT
Inside that ~740 ms hole sat the processOT line for AC0630000 (MsgID 99 Read-Ack). The frame itself was processed normally: its own gate line printed at 56.131. MsgID 99 fans out to ~9 topics in ~10 ms, which is what overruns the stream. The hole opens at the SAME fixture position in both loops (after T00630000 the next processOT is BC0630000, loop 1 and loop 2 alike), so more loops cannot recover it: the drop is caused by the deterministic burst that precedes the frame. The other diff, T10620000, WAS recovered by the second loop, so the 2-loop change earns its keep, it just is not the fix for this.

CONSEQUENCE FOR THE GATE DESIGN. The MQTT debug output is at once the source of the 219-topic half of the fingerprint AND the reason the OT half loses frames. One capture cannot carry both on 2.0.0, which also explains why 1.x never showed this: no SAT BLE traces and less concurrent publish volume. Split into two passes over the same running simulation: pass A with MQTT debug OFF owns ot/msgtypes/sources, pass B with it ON owns mqtt. merge_passes() recombines them into the existing baseline shape, so the baseline format and diff() are unchanged; a single-pass run compares only the half it owns.

SECOND DEFECT FOUND AND FIXED: toggle detection was a coin flip. The old code read the toggle menu out of the telnet banner, but live decode output splices into the banner before the menu prints (observed: the banner ends mid-separator as -----21:21:52.079893 (...). Both WARNING lines fired on the first run, meaning the runner pressed nothing and only had publish lines because an earlier run happened to leave debug on. The device D dump would give state by name but is ~290 lines and the busy telnet truncates it (8 KB in 12 s, section never arrived). Now driven off the per-toggle echo instead: Debug MQTT: true|false and Debug MQTT Gating: true|false, one short line each, identical text on both branches so it is branch-agnostic where the keypress is not (1.x gives 4 to MQTTGate, 2.0.0 gives 4 to Sensors and g to MQTTGate). Press, read back, re-press if it landed the wrong way, and treat a missing echo as wrong-key after a balancing second press. Verified on the bench through False->True->False for both flags, resolving 3 and g first try.

No baseline recorded from the failing run: it is missing a frame the firmware handled correctly, and baking that in guarantees the next run reports it as NEW.

CORRECTION to the note above: the AC0630000 drop is probabilistic, not positional. I wrote that the hole opens at the same fixture position every loop so extra loops cannot recover it. The next run disproved the strong form: AC0630000 was 0/2 loops in the first run and 1/2 in the pass-B run. Load-correlated, not deterministic. Extra loops help but do not settle it, which is what makes it a flaky gate rather than an honest failure.

The measured discriminator is better than the one I first reasoned to. Same fixture, same firmware, back-to-back on the bench S3:
  pass A (MQTT debug off): 1 spliced line, selftest halves 369/369, all 369 OT keys present in BOTH halves, and all 376 distinct fixture frames seen at least twice.
  pass B (MQTT debug on): 24 spliced lines, selftest halves 366/366, only 363 keys in both halves.
24x the stream corruption and an OT half that is not reproducible loop to loop. That is the justification for the split, not the positional argument.

Baseline recorded from the two probe captures: 369 OT keys, 143 distinct MsgIDs, 224 MQTT topics, all 6 message types, all 5 source prefixes. Previous baseline kept as baseline_coverage.json.prev until the independent verify run lands.

The 5 topics that read as NEW versus the old baseline (boiler_connected, otgw_connected, thermostat_connected, vh_diagnostic_indicator, vh_free_ventliation_status) are not new behaviour: three of them are in the set of 8 topics that appear in only one half of a two-loop pass B. The old baseline lacked them because that run lost them. Both baselines are samples of a lossy process.

KNOWN RESIDUAL RISK, mqtt section only. Pass B still reads from the same lossy telnet stream, so its topic set is sampled rather than observed. 8 of 224 topics were half-only. If the gate starts flapping on MISSING topics the fix is to read topics from the broker instead of from telnet (a subscriber sees every publish, the debug log does not) and NOT to re-record until the baseline happens to agree. Deliberately not built yet: no paho in the rig and the rest of it is stdlib-only, so this stays YAGNI until a verify run actually shows topic drift.

RESOLVED, with a semantics change the measurements forced.

The independent two-pass verify came back with the OT half EXACT (369 keys, 143 MsgIDs, zero ot/msgtypes/sources diffs) and all 9 failures in the mqtt section (215 topics vs the baseline 224). So the split fixed what it was meant to fix and exposed that the remaining half cannot be gated the way it was.

What two independent two-loop runs of identical firmware and fixture actually show:
  topic PRESENCE differed by 9. Run 2 (215) was a strict SUBSET of run 1 (224), zero spurious topics. Loss is subtractive sampling from the lossy stream, never invention.
  topic PAYLOADS differed on 0 of the 215 shared topics.
  OT keys byte-identical both runs.

So presence is not observable reliably from telnet and payloads are. diff() now returns (failures, notes): ot/msgtypes/sources gate on MISSING, NEW and CHANGED; mqtt gates on CHANGED only, with MISSING/NEW printed as informational and not setting the exit code. A payload change is exactly what a decode or publish regression produces, so mqtt keeps real signal; a topic that merely failed to be logged does not. Both callers (run_coverage_test.py and coverage_baseline.py compare) were updated together so the standalone path cannot end up stricter than the gate.

Intersection-of-runs was considered and rejected: each run drops a different random subset, so the intersection erodes with every run instead of converging.

Replaying the previously FAILING verify captures through the new semantics gives 0 failures and 9 informational notes. Baseline kept at the 224-topic union rather than re-recorded down to 215: under CHANGED-only semantics its size no longer causes flakes, and it is the closest thing available to the true published set.

NOT BUILT, deliberately, and not a blocker on this task: exact mqtt presence gating needs a lossless observer, i.e. a broker subscription instead of the debug log. That needs an MQTT password. scripts/_secrets.py resolves it from MqttPassword in the out-of-repo capture-settings.json or from $OTGW_MQTT_PASSWORD, and neither is set (capture-settings.json currently holds only BrokerHost, BrokerPort, DeviceHost, Topic, Username). capture-mqtt-debug.bat -SaveSecrets writes it. The device HTTP credential is NOT usable here: the broker is Home Assistant, a separate service, and a matching username is not authorisation. Bonus once it lands: MQTTDebugTf gates logging only, not publishing, so a subscriber works with MQTT debug OFF and the two passes collapse back into one 694s window.

BROKER SOURCE, after the maintainer confirmed the device credentials also work on the broker (2026-08-08).

This replaced the two-pass telnet split rather than adding to it. Topics now come from an MQTT subscription, which sees every publish, so presence is an observation and can be gated instead of merely reported. And because MQTTDebugTf gates the log line only and never the publish, the telnet can stay at MQTT-debug-off (what keeps decode lines intact) while the broker still sees everything: both collectors share ONE 694s window. Run time halved, 12 minutes instead of 23.

New file scripts/tests/mqtt_topic_capture.py: MQTT 3.1.1 subscribe-only client on stdlib sockets, four packet types. No paho, the rest of the rig is stdlib-only.

Five things it has to get right, each of which corrupts a baseline silently rather than failing:
1. Scope to the device under test. The broker carries otgw-AC276ECE45D8 (bench) AND otgw-2CF43257D77C (the maintainer production unit). normalise_topic() strips the uniqueid to keep baselines portable, so unscoped collection merges both devices onto the same topic names. I shipped this bug first (collector thread passed only=None while only the retained drain was scoped), caught it before the run finished, and killed the run.
2. Discard the retained flush. Subscribing replays every retained message immediately (31 to 2012 seen). Not observations of this run, and MQTT 3.1.1 cannot decline them, so drain and discard before starting the sim.
3. Read the broker from the DEVICE, not from _secrets. Stored BrokerHost is 192.168.1.11 and unreachable while the device publishes happily to homeassistant.local. /api/v2/settings and /api/v2/device/info wrap payloads in a single-key envelope and each setting is {value,type}; my first lookup missed that, returned broker=None, silently fell back to the stale host and timed out. Now raises if the device does not report a usable broker and MAC.
4. A wildcard top topic is not a prefix. _secrets topic is literally #, so <root>/# built the illegal filter #/#. A broker answers an illegal filter by closing the connection with no error packet, which surfaces much later as a bare broker closed the connection. topic_filter_for() handles it.
5. Unique client id per run, or two runs kick each other off the broker.

PREFLIGHT ADDED after a wasted run. One record run replayed the fixture FIRST FIVE LINES in a loop for the whole 694s window. Those five are all MsgID 0, so the capture looked alive (703 decode lines at correct 750ms pacing) while carrying 30 of 376 distinct frames, and --record wrote it straight over the good baseline. Not reproducible afterwards: fixture on device correct at 4230 bytes, fresh start replays perfectly, upload-then-immediately-start does not trigger it. Most likely transient state from killing the previous run mid-capture. preflight_replay() now samples 25s after start and requires at least 12 distinct fixture frames (a healthy replay does ~33), so a stuck replay costs 25 seconds instead of 12 minutes plus a poisoned baseline. Baseline was recovered by re-recording from the two probe captures, which reproduces it exactly.

Gating is source-aware: the fingerprint records mqtt_source and mqtt presence is gated only when baseline AND run both came from the broker. A broker baseline compared against a telnet run would otherwise report every dropped log line as a regression. --topics telnet keeps the old lossy path available with presence reported but not gated.

Baseline re-recorded from the broker: 369 OT keys, 143 distinct MsgIDs, 225 MQTT topics. The broker sees one topic MORE than the best telnet run ever managed (224, with a low of 215), which is the lossless observation showing itself.
<!-- SECTION:NOTES:END -->
