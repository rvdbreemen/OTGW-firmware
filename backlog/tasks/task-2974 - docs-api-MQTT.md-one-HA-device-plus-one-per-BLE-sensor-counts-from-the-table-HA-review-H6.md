---
id: TASK-2974
title: >-
  docs/api/MQTT.md: one HA device plus one per BLE sensor, counts from the table
  (HA review H6)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T15:58:00Z'
updated_date: '2026-11-20T16:04:00Z'
labels:
  - docs
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1106000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2970 HA review (docs/reviews/2026-11-20_ux-ha-entities/README.md, H6); umbrella TASK-2970. docs/api/MQTT.md:1535-1560 still describes five HA devices (ADR-122) and :1255-1256 lists 400 sensors and 109 binary sensors. The code creates one gateway device (MQTTHaDiscovery.cpp:2440-2441) plus one device per BLE sensor (MQTTstuff.ino:3780-3825), with 405 sensor and 113 binary rows (MQTTHaDiscovery.cpp:869, :1443).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT.md describes one gateway device plus one device per BLE sensor, with via_device to the gateway
- [x] #2 The entity counts in MQTT.md are derived from the table by a script or test, or replaced by a statement that discovery follows the message IDs the boiler answers
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs/api/MQTT.md 'HA Discovery Five-Device Topology (ADR-122)' replaced by 'HA Discovery: one device per gateway (ADR-140)': one gateway device {nodeId} (MQTTHaDiscovery.cpp device block around :2420-2447, full block on the first entity) plus one child device per BLE sensor {nodeId}_ble_<mac> with via_device to the gateway (MQTTstuff.ino around :3750-3830, ADR-148); unique_id {nodeId}-<source prefix><label>[_thermostat|_boiler] (MQTTHaDiscovery.cpp around :2509-2520) with the prefixes esp_/pic_/otd_/sat_/sensors_ (haSourcePrefix, around :2409-2417); the ADR-106 topic-naming subsection kept. The internal bLegacyMode has no setting (no settings key), so the doc no longer describes it. AC2: Entity Inventory counts corrected (sensor 400 -> 405, binary_sensor 109 -> 113, indexed 72 -> 76) and kept in sync by tests/test_mqtt_doc_entity_counts.py, which reads MQTT_HA_SENSOR_COUNT, MQTT_HA_BINSENSOR_COUNT and MQTT_HA_BINSENSOR_INDEXED_COUNT from MQTTHaDiscovery.cpp and fails when MQTT.md differs or still describes the five-device layout. Red on the old MQTT.md (MQTT_DOC=<HEAD copy>: FAILED failures=4), green on the new (Ran 4 tests OK); picked up by scripts/run_tests.py (tests/test_*.py). Not verified: the climate (2) and number (1) rows were not re-counted; nothing checked against a live Home Assistant.
<!-- SECTION:FINAL_SUMMARY:END -->
