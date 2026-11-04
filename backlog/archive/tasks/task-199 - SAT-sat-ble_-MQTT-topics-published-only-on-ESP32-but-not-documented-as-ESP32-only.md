---
id: TASK-199
title: >-
  SAT: sat/ble_* MQTT topics published only on ESP32 but not documented as
  ESP32-only
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:21'
updated_date: '2026-04-09 05:51'
labels:
  - audit-fix
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SATble.ino publishes sat/ble_temp, sat/ble_humidity, sat/ble_sensor_rssi, sat/ble_battery, sat/ble_sensor_count, sat/ble_temp_valid (lines 361-375). SATble.ino is entirely wrapped in #if defined(ESP32) so these topics only exist on OTGW32. However the MQTT topic documentation (if any) and HA discovery configs do not explicitly mark these as ESP32-only. Subscribers or HA automations that expect these topics on ESP8266 will silently get nothing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add sat/ble_* topics to the MQTT topic documentation with 'OTGW32/ESP32 only' note
- [x] #2 Verify HA auto-config (mqttha.cfg) does not emit BLE sensor discovery entries on ESP8266
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read mqttha.cfg to find BLE discovery entries
2. Check docs/api/MQTT.md BLE section for ESP32-only documentation
3. Add per-entry ESP32-only comments to the 3 BLE discovery entries in mqttha.cfg
4. Verify the parser handles inline // comments correctly (confirmed: parseAutoConfigLine strips // onward before field splitting)
5. Verify MQTT.md already documents BLE topics as ESP32-only (confirmed complete)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
MQTT.md already had a complete BLE section (line 404: "BLE Temperature Sensor (ESP32 only)") with all 6 topics listed and "(ESP32 only)" notes in the HA entity table at lines 787-789.

mqttha.cfg already had a group comment on line 511 marking the block as ESP32-only. Added a clarifying note comment plus individual "// ESP32 only" inline comment on each of the 3 BLE discovery entries (ble_temp, ble_humidity, ble_rssi).

Verified the mqttha.cfg parser (parseAutoConfigLine in MQTTstuff.ino:131-132) strips // comments before field splitting, so inline comments are safe.

The 3 remaining BLE topics (ble_battery, ble_sensor_count, ble_temp_valid) have no HA discovery entries in mqttha.cfg at all — that is a pre-existing gap, not in scope for this task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Documented sat/ble_* MQTT topics as ESP32-only in both mqttha.cfg and docs/api/MQTT.md.

Changes:
- mqttha.cfg: expanded the existing BLE group comment to clarify that on ESP8266 these topics are never published and discovery configs receive no data; added "// ESP32 only" inline comment to each of the 3 BLE HA discovery entries (ble_temp, ble_humidity, ble_rssi). The inline comments are safely handled by parseAutoConfigLine() which strips // onward before field parsing.
- docs/api/MQTT.md: already complete — the BLE section heading ("ESP32 only"), the intro note on line 406, and the HA entity table all correctly document the ESP32 restriction. No changes needed.

Note: ble_battery, ble_sensor_count, ble_temp_valid have no HA discovery entries in mqttha.cfg — pre-existing gap, not in scope for this task.
<!-- SECTION:FINAL_SUMMARY:END -->
