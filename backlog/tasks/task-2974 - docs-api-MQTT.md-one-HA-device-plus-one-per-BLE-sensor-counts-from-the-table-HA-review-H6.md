---
id: TASK-2974
title: >-
  docs/api/MQTT.md: one HA device plus one per BLE sensor, counts from the table
  (HA review H6)
status: To Do
assignee:
  - '@otgw-docs'
created_date: '2026-11-20T15:58:00Z'
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
- [ ] #1 MQTT.md describes one gateway device plus one device per BLE sensor, with via_device to the gateway
- [ ] #2 The entity counts in MQTT.md are derived from the table by a script or test, or replaced by a statement that discovery follows the message IDs the boiler answers
<!-- AC:END -->
