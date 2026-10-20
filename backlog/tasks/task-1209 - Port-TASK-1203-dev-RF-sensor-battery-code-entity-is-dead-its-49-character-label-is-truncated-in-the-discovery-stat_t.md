---
id: TASK-1209
title: >-
  Port TASK-1203 (dev): RF sensor battery-code entity is dead, its 49-character
  label is truncated in the discovery stat_t
status: Done
assignee:
  - '@claude'
created_date: '2026-10-04 10:46'
updated_date: '2026-10-20 04:31'
labels:
  - mqtt
  - ha-discovery
  - port
dependencies: []
priority: low
ordinal: 240000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Cross-line check from dev TASK-1203 (2026-10-04), verified in 1.x code by a read-only review. mqtt_configuratie.cpp:166 defines the 49-character label RFSensorStatusInformation_battery_indication_code (sensor row at :945, OT ID 98). composeSensorPayload copies it into char label[48] (:2038/:2041) and writes that into stat_t (:2096), so the state topic ends in ..._battery_indication_co. OTGW-Core.ino:2854 publishes the value under the full name, so the Home Assistant entity never receives a value. It is the only label over 47 characters (the next is 46).
The dev fix (commit 8ad028cac): the stat_t label buffer goes to 64 bytes in composeSensorPayload and composeBinSensorPayload, while idLabel (uniq_id) and the topic builders keep their 48-byte buffers. The existing HA entity therefore keeps its uniq_id and entity_id and starts receiving values, with no orphan and no migration.
Related latent weakness, no bug today: buildBinSensorDiscoveryTopic (:2240-2249) does not sanitize the label (dev TASK-1204), but no 1.x binary-sensor label contains '/'.
Dev TASK-1202 and TASK-1206 do not apply to 1.x: the reset button and selects write cmd_t, and the PIC controls are pseudo-ID 251 with no earlier section setting result.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The battery-code sensor's discovery stat_t equals the topic its value is published on (shown on 1.x, old vs fix)
- [x] #2 uniq_id and config topic unchanged, so the existing entity is kept
- [x] #3 Build green for the esp8266 target; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 06:3x fix-build capture (bench .88.68, 1.7.8-beta.5+d92c03f, built from the working tree that became 52d052070; temp Mosquitto 192.168.88.32:1884, 300 s, script scratchpad t1209/capture.sh, output fix_disc.txt 436 lines):
- config topic homeassistant/sensor/otgw-84F3EB22B8E1/RFSensorStatusInformation_battery_indication_co/config (unchanged)
- uniq_id otgw-84F3EB22B8E1-RFSensorStatusInformation_battery_indication_co (unchanged)
- stat_t OTGW/value/otgw-84F3EB22B8E1/RFSensorStatusInformation_battery_indication_code (was ..._co on 84be30d, old_disc.txt)
- value topic name comes from OTGW-Core.ino:2854 (full _code name); no RF battery value was published in the 300 s window, so the value message itself was not captured live.
- Bench restored to homeassistant.local:1883, mqttconnected true, bootcount 8 (TASK-1261 24 h window intact).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The RF battery-code sensor now gets its value in Home Assistant: its discovery stat_t carries the full label.

Change (52d052070, mqtt_configuratie.cpp composeSensorPayload): label buffer 48 -> 64 bytes. The 49-char label RFSensorStatusInformation_battery_indication_code was cut to ..._co in stat_t, while OTGW-Core.ino publishes the value under the full name. idLabel stays 48, so uniq_id and config topic do not change and the existing HA entity is kept. Port of dev task 1203.

Evidence (bench .88.68, local Mosquitto capture):
- old 1.7.8-beta.5+84be30d: stat_t .../RFSensorStatusInformation_battery_indication_co
- fix 1.7.8-beta.5+d92c03f: stat_t .../RFSensorStatusInformation_battery_indication_code; uniq_id and config topic identical to old.
- build.bat "Build completed successfully!", evaluate.py --quick 37/37.

Not verified: a live RF battery value arriving on the _code topic (no RF data in the 300 s window); the topic name is taken from the publish call at OTGW-Core.ino:2854. CHANGELOG bullet under [Unreleased].
<!-- SECTION:FINAL_SUMMARY:END -->
