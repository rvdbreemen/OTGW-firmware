---
id: TASK-1059
title: >-
  feat-2.0.0: port TASK-1058 — republish on-change gated MQTT state when HA
  comes back online
status: Done
assignee:
  - '@claude'
created_date: '2026-08-07 21:40'
updated_date: '2026-10-03 13:51'
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
- [x] #3 hvac_mode and hvac_action are re-sent after an HA restart without a reboot
- [x] #4 All other on-change gated values are re-sent (MsgID slots, status/statusVH bits+bytes, ASF/RBP/RO)
- [x] #5 No discovery-config republish is introduced; the ADR-100 JIT discovery decision stays intact
- [x] #6 publishHvacMode/publishHvacAction latch their cache only on a confirmed send, else fall back to the unset sentinel
- [x] #7 MQTTharebootdetection is still parsed and written but gates nothing and is absent from the UI
- [x] #8 The republish burst introduces no re-entrancy hazard on the async MQTT path (ADR-174 branch-local condition), confirmed by inspection or on-device test
- [x] #9 Build green for the relevant esp32 target, verified on artifact freshness and the per-env SUCCESS line
- [x] #10 python evaluate.py --quick shows no new failures
- [x] #11 Field validation on 2.0.0 hardware across a Home Assistant restart
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-174 async re-entrancy condition (AC#8) resolved by inspection, no on-device test needed:
espMqttClient is constructed with UseInternalTask::NO (MQTTstuff.ino:206). The contract is documented at :196-204 - with NO, the engine is pumped only by the explicit MQTTclient.loop() inside handleMQTT(), so onMessage/onConnect callbacks run on the same cooperative loop as doBackgroundTasks(), NOT on async_tcp. The new call site at MQTTstuff.ino:817 is therefore in the identical task context as the already-shipped reconnect caller at :1325.
Noted but out of scope: restAPI.ino:1993 calls requestMQTTRepublishAll() from the ESPAsyncWebServer handler, which DOES run on async_tcp while the loop task reads/writes the same trackers. Pre-existing, not introduced here.
Build: build.bat, all three envs relinked fresh with githash dd5a701 (classic 23:59:32, otgw32 23:56:32, combo 00:02:34). Evaluator 68/76 passed, 0 failed, 1 warning (STATUS_BURST_COOLDOWN_MS bound: boards.h not found) which is pre-existing and unrelated to this diff.

2026-09-30 docs aligned with the shipped behaviour (docs-only commit, no bump): docs/api/MQTT.md, docs/api/openapi.yaml, docs/c4/c4-code-mqtt.md, c4-component-integration-layer.md, c4-container.md and docs/manuals/nl/h10-bijlagen.md. An HA restart republishes STATE (ADR-174), not discovery; the reconnect republish only runs after >300 s offline; MQTTharebootdetection gates nothing; the daily heal (ADR-170) replaced the automatic verify; drip timing; the REST republish is queued for loop() (TASK-1176). Verification (workflow wf_fe6c4173-ec6, WP4 + review + fixup): a sentence inventory maps all 166 keyword lines of the six docs to code anchors (286 anchors, 0 failures) on both the base and current dev; 17 file:line citations checked for staleness on dev (0 stale); 39 regression patterns for the previously false sentences find nothing; openapi.yaml parses with the same 66 paths.

AC#3 ON 2.0.0 HARDWARE, 2026-10-03. OTGW32 192.168.88.61 running 2.0.0-alpha.404+1d9ae71. MQTT to the Docker test-rig mosquitto 2.1.2 (192.168.88.32), mqtthaprefix homeassistant. OT traffic came from the shipped /otgw_simulation.log replay (750 ms per line; a master MsgID 0 frame every 22.5 s).
HA restart signal: the rig's Home Assistant has no MQTT integration configured (core.config_entries has analytics, backup, go2rtc, google_translate, met, radio_browser, shopping_list and sun only). So the restart was replayed as the exact MQTT sequence HA produces: homeassistant/status 'offline' (will) and 10 s later 'online' (birth), both non-retained, sent with mosquitto_pub. The firmware only sees HA through these messages.
Timeline (mosquitto_sub, OTGW/value/otgw-1020BA21B4F8/hvac_* plus homeassistant/status):
- 10:18:51 hvac_mode heat, hvac_action heating (first publish)
- 10:18:55 hvac_action idle (a change)
- 10:18:55-10:20:21: nothing, 86 s with about 4 master frames. This is the control: unchanged values are not re-sent.
- 10:20:21 homeassistant/status offline
- 10:20:30 homeassistant/status online
- 10:20:53 hvac_mode heat and 10:20:54 hvac_action idle: re-sent with unchanged values, about 23 s after 'online', i.e. at the next master frame.
This is not the hvac heartbeat: HVAC_HEARTBEAT_INTERVAL_SEC is 300 (OTGW-Core.ino:2303), so the next heartbeat was due at about 10:23:51. No reboot: bootcount 5 -> 5, uptime 01:36 -> 01:39.
AC#11 (field validation across a real HA restart) stays open. It needs a real HA with MQTT: either the MQTT integration added to the rig HA (broker 'mosquitto', port 1883, no auth; HA UI login required) or a restart of the production HA with the bench connected.

AC#11 field validation, 2026-10-03, OTGW32 bench on 2.0.0-alpha.404+220fca7 (current dev firmware), against the laptop test rig: Home Assistant 2026.6.3 in Docker plus Mosquitto. The maintainer authorized changes to the rig. The rig HA had no MQTT integration, so it got one: a config entry for broker mosquitto:1883 was added to its .storage, in the format the HA 2026.6.3 MQTT flow writes (VERSION 2.1, data {broker, port}, empty options). HA connected 3 s after start, and its birth and will messages are the defaults. The original file is backed up next to the evidence.
Frame source: the onboard OT replay (/api/v2/simulate). No boiler or thermostat is attached; the replay emits a MsgID 0 master frame about every 22.6 s.
Run t1059_sim2, a real HA restart via 'docker restart homeassistant'. Times are relative to the restart; telnet uses the device clock path and MQTT the broker clock:
- +1.3 s: the device receives homeassistant/status = offline.
- +14.3 s: master frame, status_master skip[no-change].
- +26.1 s: the device receives homeassistant/status = online (HA birth).
- +36.9 s: the next master frame gives status_master publish[force], then Sending MQTT hvac_mode [heat] (+37.0 s) and hvac_action [idle] (+37.8 s). The broker received status_master, hvac_mode and hvac_action at +37.0 to +37.7 s.
- +59.5 and +82.1 s: skip[no-change]; +104.6 s: publish[interval] for status_master only, without hvac_mode.
- bootcount 10 before and after; uptime kept running, so there was no reboot. PASS.
Run t1059_sim: the same result (birth +23.0 s, then the three topics together 13 s later).
Run t1059 (without the replay): birth seen, but no OT frames, so nothing to publish. That shows the republish rides on the next master frame; it is not an immediate burst.
Evidence (out of the repo): %LOCALAPPDATA%/OTGW-capture/task1059/ (ha_restart_1059.py, MQTT and telnet transcripts, result.json per run).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of 1.x TASK-1058 to 2.0.0. When Home Assistant comes back online (homeassistant/status offline then online), the firmware re-publishes its on-change-gated MQTT state, hvac_mode and hvac_action included, without a device reboot (fix f638bbaff, ADR-174 accepted in dd5a70153, docs df0094ac0). A replayed or retained 'online' without a preceding 'offline' does not trigger it, and no discovery republish is added (ADR-100 stays intact).

Evidence:
- AC#1-#10: as recorded in the notes.
- AC#11, field validation on 2.0.0 hardware: the OTGW32 bench on alpha.404+220fca7 and a real Home Assistant 2026.6.3 container restart on the laptop rig, with the onboard OT replay as frame source. HA's birth 'online' reached the device. At the next master frame the gate logged status_master publish[force], and hvac_mode and hvac_action were sent; the broker received all three about 11 s after the birth. bootcount was unchanged.
- A run without OT frames shows that the republish waits for the next master frame.

Transcripts are kept out of the repo under %LOCALAPPDATA%/OTGW-capture/task1059/.
<!-- SECTION:FINAL_SUMMARY:END -->
