---
id: TASK-1059
title: >-
  feat-2.0.0: port TASK-1058 — republish on-change gated MQTT state when HA
  comes back online
status: To Do
assignee: []
created_date: '2026-08-07 21:40'
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
- [ ] #1 homeassistant/status offline->online triggers requestMQTTRepublishAll()
- [ ] #2 A replayed or retained online without a preceding offline does NOT trigger a republish
- [ ] #3 hvac_mode and hvac_action are re-sent after an HA restart without a reboot
- [ ] #4 All other on-change gated values are re-sent (MsgID slots, status/statusVH bits+bytes, ASF/RBP/RO)
- [ ] #5 No discovery-config republish is introduced; the ADR-100 JIT discovery decision stays intact
- [ ] #6 publishHvacMode/publishHvacAction latch their cache only on a confirmed send, else fall back to the unset sentinel
- [ ] #7 MQTTharebootdetection is still parsed and written but gates nothing and is absent from the UI
- [ ] #8 The republish burst introduces no re-entrancy hazard on the async MQTT path (ADR-174 branch-local condition), confirmed by inspection or on-device test
- [ ] #9 Build green for the relevant esp32 target, verified on artifact freshness and the per-env SUCCESS line
- [ ] #10 python evaluate.py --quick shows no new failures
- [ ] #11 Field validation on 2.0.0 hardware across a Home Assistant restart
<!-- AC:END -->
