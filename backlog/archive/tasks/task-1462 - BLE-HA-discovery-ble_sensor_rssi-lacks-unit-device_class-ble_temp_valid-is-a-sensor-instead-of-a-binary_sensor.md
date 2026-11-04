---
id: TASK-1462
title: >-
  BLE HA discovery: ble_sensor_rssi lacks unit/device_class, ble_temp_valid is a
  sensor instead of a binary_sensor
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:21:00Z'
updated_date: '2026-11-04T05:14:00Z'
labels:
  - ble
  - mqtt
  - ha-discovery
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 622000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1313 (rssi: no unit, no device_class) and :1316 (temp_valid announced as sensor). Changing the component of ble_temp_valid changes its entity id: retire the old sensor config.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 rssi announced with unit dBm and device_class signal_strength; ble_temp_valid announced as binary_sensor and the old sensor config retired; discovery harness covers both
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: MQTTHaDiscovery.cpp:1316 rssi no unit/device_class, :1319 temp_valid is a sensor. Agent-doable (old config must be retired).

2026-11-03 (sonnet agent; lead reran rebased on dev): rssi announced with dBm / signal_strength / measurement (new HaDeviceClass::signal_strength, HaUnit::dBm); sat/ble_temp_valid moved from the sensor table to the binary table (SAT_BIN 251, payload true/false matching publishIfChangedB), indexes shifted (MQTT_HA_SENSOR_COUNT 400->399, BIN 110->111, INDEXED 73->74); old sensor config retired via kRetiredSatEntities (empty retained on homeassistant/sensor/<node>/sat_sat_ble_temp_valid/config). tests/test_ble_discovery_1462.py: old 4/5 fail, fix 5 OK; test_ha_discovery_json PASS; test_dhw_water_meter anchor updated, PASS; evaluate 0 failed (HA index gates). Tag beta.62; bench (broker config topics) pending in the shared build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
BLE RSSI is announced in dBm with signal_strength and sat/ble_temp_valid as a binary_sensor; the old sensor config is retired. Evidence: tests/test_ble_discovery_1462.py old 4/5 fail, fix 5 OK; HA index gates green; bench broker (both units, beta.62): binary_sensor/<node>/sat_sat_ble_temp_valid with pl_on true / pl_off false, sensor rssi with dBm/signal_strength/measurement, old sensor config cleared. Shipped beta.62.
<!-- SECTION:FINAL_SUMMARY:END -->
