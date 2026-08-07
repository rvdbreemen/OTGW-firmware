---
id: TASK-1059
title: >-
  feat-2.0.0: port TASK-1058 — republish on-change gated MQTT state when HA
  comes back online
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-07 21:40'
updated_date: '2026-08-07 22:04'
labels:
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 253000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x TASK-1058 / ADR-088 to the 2.0.0 line, governed by ADR-174 (Accepted 2026-08-07). After a Home Assistant restart every entity backed by an on-change gated MQTT value sits at unknown: discovery configs are retained so HA rebuilds the entities, but state topics are not retained and most values publish only on change. Fix: the homeassistant/status offline->online transition calls requestMQTTRepublishAll() (src/OTGW-firmware/OTGW-Core.ino:1788), which resets every on-change gate; hvac_mode/hvac_action follow transitively via forcePublish. Remove the !settings.mqtt.bHaRebootDetect pre-arm at src/OTGW-firmware/MQTTstuff.ino:802-806 so bHAcycle is armed only by an observed offline, making a retained HA birth message replayed on reconnect a no-op. Handler lives at src/OTGW-firmware/MQTTstuff.ino:800-818. Deprecate MQTTharebootdetection: keep parsing/writing it, remove from the UI, gate nothing. Also port the hvac latch fix from the 1.x commit: publishHvacMode/publishHvacAction must latch their RAM cache only on a confirmed sendMQTTData, falling back to the -1 unset sentinel, because forcePublish is one-shot and cleared before the fan-out runs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 homeassistant/status offline->online triggers requestMQTTRepublishAll()
- [x] #2 A replayed or retained online without a preceding offline does NOT trigger a republish
- [ ] #3 hvac_mode and hvac_action are re-sent after an HA restart without a reboot
- [x] #4 All other on-change gated values are re-sent (MsgID slots, status/statusVH bits+bytes, ASF/RBP/RO)
- [x] #5 No discovery-config republish is introduced; the ADR-100 JIT discovery decision stays intact
- [x] #6 publishHvacMode/publishHvacAction latch their cache only on a confirmed send, else fall back to the unset sentinel
- [x] #7 MQTTharebootdetection is still parsed and written but gates nothing and is absent from the UI
- [x] #8 The republish burst introduces no re-entrancy hazard on the async MQTT path (ADR-174 branch-local condition), confirmed by inspection or on-device test
- [x] #9 Build green for the relevant esp32 target, verified on artifact freshness and the per-env SUCCESS line
- [x] #10 python evaluate.py --quick shows no new failures
- [ ] #11 Field validation on 2.0.0 hardware across a Home Assistant restart
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-174 async re-entrancy condition (AC#8) resolved by inspection, no on-device test needed:
espMqttClient is constructed with UseInternalTask::NO (MQTTstuff.ino:206). The contract is documented at :196-204 - with NO, the engine is pumped only by the explicit MQTTclient.loop() inside handleMQTT(), so onMessage/onConnect callbacks run on the same cooperative loop as doBackgroundTasks(), NOT on async_tcp. The new call site at MQTTstuff.ino:817 is therefore in the identical task context as the already-shipped reconnect caller at :1325.
Noted but out of scope: restAPI.ino:1993 calls requestMQTTRepublishAll() from the ESPAsyncWebServer handler, which DOES run on async_tcp while the loop task reads/writes the same trackers. Pre-existing, not introduced here.
Build: build.bat, all three envs relinked fresh with githash dd5a701 (classic 23:59:32, otgw32 23:56:32, combo 00:02:34). Evaluator 68/76 passed, 0 failed, 1 warning (STATUS_BURST_COOLDOWN_MS bound: boards.h not found) which is pre-existing and unrelated to this diff.
<!-- SECTION:NOTES:END -->
