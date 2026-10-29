---
id: TASK-1462
title: >-
  BLE HA discovery: ble_sensor_rssi lacks unit/device_class, ble_temp_valid is a
  sensor instead of a binary_sensor
status: To Do
assignee: []
created_date: '2026-10-29T07:21:00Z'
labels:
  - ble
  - mqtt
  - ha-discovery
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 622000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. MQTTHaDiscovery.cpp:1313 (rssi: no unit, no device_class) and :1316 (temp_valid announced as sensor). Changing the component of ble_temp_valid changes its entity id: retire the old sensor config.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 rssi announced with unit dBm and device_class signal_strength; ble_temp_valid announced as binary_sensor and the old sensor config retired; discovery harness covers both
<!-- AC:END -->
