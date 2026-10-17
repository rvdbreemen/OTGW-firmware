---
id: TASK-1240
title: >-
  Two small code findings from the C4 refresh: case-sensitive MQTT side effect,
  dead S0 autoconfigure call
status: Done
assignee:
  - '@claude'
created_date: '2026-10-16 04:18'
updated_date: '2026-10-17 14:37'
labels:
  - bug
  - settings
  - s0
dependencies: []
priority: low
ordinal: 378000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while refreshing docs/c4 (TASK-1239), verified by reading the source on alpha.427. (1) settingStuff.ino updateSetting(): the MQTT restart side effect is set by strstr_P(field, PSTR("mqtt")), which is case-sensitive, so a setting key written as e.g. 'MQTTbroker' (settings.ini spelling) through a path that does not lowercase it would change the value without restarting MQTT. The v2 UI sends lowercase names, so the impact is probably limited to other callers (MQTT set commands, scripts). Needs a reproduction before a fix. (2) s0PulseCount.ino calls sensorAutoConfigure(OTGWs0dataid, true, ""), which returns immediately on an empty sensor id (MQTTstuff.ino), so the call is dead; S0 discovery (id 245) already goes through the generic discovery drip. Remove the call or document why it stays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Finding 1 reproduced (or shown unreachable) on the bench, with the callers that can pass a mixed-case key listed
- [x] #2 If reachable: the side-effect match is case-insensitive and a mixed-case key restarts MQTT (old-vs-fix shown)
- [x] #3 The dead S0 sensorAutoConfigure call is removed, S0 discovery still appears on the broker
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Finding 1 reachable: postSettings() (restAPI.ino) passes the JSON name verbatim to updateSetting() after the case-insensitive isKnownSetting(), so POST {"name":"MQTTbrokerPort"} (the settings.ini spelling) saved the value without the MQTT restart. Other callers use fixed keys (MQTT kSatMqttCmds passthrough keys are all SAT*, OTD*/SATble* keys contain no mqtt); readSettings() clears side effects after load. Fix: strncasecmp_P(field, PSTR("mqtt"), 4) == 0; all 17 MQTT keys start with MQTT (grep of settingStuff.ino). Old-vs-fix, telnet probe .tmp/bench-1240/mqtt_side_effect_probe.py on the Classic-S3, real port change each POST: alpha.427 MQTTbrokerPort -> flush but no 'Restarting MQTT (deferred)' 2/2, lowercase -> restart 2/2 (old-a427 log .tmp/bench-1240/old-a428.txt; device ran alpha.427+cdf90cf firmware); alpha.429 MQTTbrokerPort -> restart 2/2, lowercase 2/2 (fix-a429.txt). Finding 2: sensorAutoConfigure(OTGWs0dataid, true, "") returns before any work (config-done check, then empty-id check, MQTTstuff.ino:2794-2795); line removed. AC3: temporary mosquitto on the PC (port 1885, the Classic's broker IP is this PC), S0 counter enabled: all 4 id-245 configs published (sensors_s0powerkw, s0pulsecount, s0pulsecounttot, s0pulsetime; .tmp/bench-1240/mqtt-capture.txt). Bench restored (S0 off, port 1883). Gates: build.bat --target esp32-combo SUCCESS, evaluate --quick 0 FAIL.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Two findings from the C4 refresh. (1) A settings POST with a mixed-case MQTT key (e.g. MQTTbrokerPort, the settings.ini spelling) changed the value without restarting MQTT, because the side-effect test was a case-sensitive strstr_P. It is now a case-insensitive prefix test. Old-vs-fix on the Classic-S3 over telnet: restart 0/2 -> 2/2. (2) The dead S0 sensorAutoConfigure call is removed; S0 discovery still reaches the broker (4 id-245 configs captured). alpha.429.
<!-- SECTION:FINAL_SUMMARY:END -->
