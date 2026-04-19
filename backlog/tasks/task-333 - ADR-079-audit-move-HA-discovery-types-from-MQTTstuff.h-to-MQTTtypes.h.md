---
id: TASK-333
title: 'ADR-079 audit: move HA-discovery types from MQTTstuff.h to MQTTtypes.h'
status: To Do
assignee: []
created_date: '2026-04-19 18:34'
labels:
  - architecture
  - adr-079
  - cleanup
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.h has grown to 361 lines and contains 9 MQTT type declarations that belong in MQTTtypes.h per ADR-079 (types live in <Component>types.h, not in the functional stuff.h). Types to move: HaDeviceClass, HaUnit, HaStateClass, HaIcon, HaEntityCat enums + MqttHaSensorCfg, MqttHaBinSensorCfg, HaDiscoveryContext, MqttJsonWriter structs/classes. After the move MQTTstuff.h becomes pure interface (macros + function decls + PubSubClient helpers) and MQTTtypes.h is the single canonical location for every MQTT type.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 5 Ha* enums and all 4 MqttHa*/HaDiscoveryContext/MqttJsonWriter structs live in MQTTtypes.h
- [ ] #2 MQTTstuff.h contains no struct/class/enum definitions — only macros, function decls, and helpers
- [ ] #3 Both ESP8266 and ESP32-S3 build clean; MQTTHaDiscovery.cpp still compiles (it already includes MQTTstuff.h)
- [ ] #4 evaluate.py check_adr_gates remains clean; no regression in tests/test_evaluate.py
<!-- AC:END -->
