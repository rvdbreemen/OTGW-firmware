---
id: TASK-1203
title: >-
  RF sensor battery-code entity is dead: discovery truncates its 49-character
  label to 47 characters while the value is published under the full name
status: Done
assignee:
  - '@claude'
created_date: '2026-10-03 17:54'
updated_date: '2026-10-04 08:57'
labels:
  - ha-discovery
  - bug
  - mqtt
dependencies: []
priority: medium
ordinal: 325000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier, confirmed 2026-10-03 on the rig broker (alpha.404). Table row MQTTHaDiscovery.cpp:1113 (label RFSensorStatusInformation_battery_indication_code, 49 characters) is copied into char label[48] in composeSensorPayload (:2386-2389) and into labelBuf[48] in buildSensorDiscoveryTopic (:2634-2635). Retained config: stat_t = OTGW/value/<node>/RFSensorStatusInformation_battery_indication_co. The value is published by publish_mqtt_u8_code_and_text_topics(F("RFSensorStatusInformation_battery_indication_code")) at OTGW-Core.ino:3471, under the full 49-character name. So the Home Assistant entity never receives a value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The discovery stat_t of the battery-code sensor equals the topic its value is published on (host test that fails on the old code)
- [x] #2 No other discovery label is truncated: a host check over the whole sensor and binary-sensor tables compares every label length with the buffers
- [x] #3 The uniq_id consequence is decided and recorded: whether the fixed label changes the uniq_id, and what happens to the existing dead entity in Home Assistant
- [x] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix: composeSensorPayload() and composeBinSensorPayload() copy the label into char label[64] (was 48), and that buffer feeds stat_t. idLabel (uniq_id) and the topic builders keep their 48-byte buffers.
Evidence (host harness test/host/test_ha_discovery_json.py --old-rev 8795bacc0, RESULT: PASS):
- AC#1: the RF battery-code sensor's stat_t is OTGW/value/<node>/RFSensorStatusInformation_battery_indication_code on the fix; OLD has ..._co.
- AC#2: a static check reads every table label (ha_lbl_* and DECLARE_SAT_DISCOVERY_STRINGS) and the composers' char label[N]. No label is longer than N-1 (longest 49; next 46).
- AC#3, decided: the uniq_id is NOT changed. It stays otgw-<mac>-pic_RFSensorStatusInformation_battery_indication_co, and so does the config topic. The existing HA entity, which received no value, keeps its entity_id and customisations and starts receiving values. Changing the uniq_id would create a second entity and leave the dead one behind with a retained config that needs a migration. The old-vs-fix diff proves it: every payload is byte-identical except the 12 RF battery-code payloads, whose only change is the completed stat_t (uniq_id bytes identical).

Rig 2026-10-04, alpha.411+fc27f1a: after a republish, the retained homeassistant/sensor/otgw-1020BA21B4F8/boiler_RFSensorStatusInformation_battery_indication_co/config carries stat_t OTGW/value/otgw-1020BA21B4F8/RFSensorStatusInformation_battery_indication_code with the unchanged uniq_id otgw-1020BA21B4F8-otd_RFSensorStatusInformation_battery_indication_co. The HA entity sensor.opentherm_gateway_otgw_rf_sensor_battery_code, created 2026-10-03 13:58Z, is kept. The bench has no RF sensor (MsgID 98), so a live value cannot be shown here. AC#4: esp32, esp32-classic and esp32-combo SUCCESS (alpha.411); evaluate.py --quick 0 failures.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The RF sensor battery-code entity now receives its value. The sensor and binary-sensor composers copied the label into a 48-byte buffer that feeds stat_t, which cut the 49-character RFSensorStatusInformation_battery_indication_code to ..._co, while the value is published under the full name. The stat_t buffer is now 64 bytes. The uniq_id and config topic deliberately keep their 48-byte truncation, so the existing HA entity is kept and simply starts receiving values: no orphan entity and no migration. Evidence: host harness old-vs-fix PASS (OLD ..._co, FIX full name; a static check confirms no table label exceeds the buffer; the diff shows only the 12 RF battery-code payloads changed, and only in stat_t); rig retained config carries the full stat_t with the unchanged uniq_id and the existing entity is kept; builds for 3 envs green, evaluate clean.
<!-- SECTION:FINAL_SUMMARY:END -->
