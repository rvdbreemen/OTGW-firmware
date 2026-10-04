---
id: TASK-1209
title: >-
  Port TASK-1203 (dev): RF sensor battery-code entity is dead, its 49-character
  label is truncated in the discovery stat_t
status: To Do
assignee: []
created_date: '2026-10-04 10:46'
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
- [ ] #1 The battery-code sensor's discovery stat_t equals the topic its value is published on (shown on 1.x, old vs fix)
- [ ] #2 uniq_id and config topic unchanged, so the existing entity is kept
- [ ] #3 Build green for the esp8266 target; evaluate.py --quick shows no new failures
<!-- AC:END -->
