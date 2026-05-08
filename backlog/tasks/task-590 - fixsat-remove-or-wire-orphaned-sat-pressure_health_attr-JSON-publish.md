---
id: TASK-590
title: 'fix(sat): remove or wire orphaned sat/pressure_health_attr JSON publish'
status: To Do
assignee: []
created_date: '2026-05-08 17:13'
labels:
  - sat
  - mqtt
  - ha-discovery
dependencies: []
references:
  - 'src/OTGW-firmware/SATcontrol.ino:2042-2062'
  - src/OTGW-firmware/MQTTHaDiscovery.cpp
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SATcontrol.ino:2042-2062 publishes a JSON blob on `sat/pressure_health_attr` but no HA auto-discovery entry registers this as a `json_attributes_topic` for the `sat/pressure_health` binary sensor. The payload is consumed by nobody.

JSON payload fields: pressure, smoothed_pressure, pressure_drop_rate_bar_per_hour, last_pressure, last_pressure_timestamp, last_seen_pressure_timestamp.

Resolution options:
1. Add json_attributes_topic to the sat/pressure_health binary sensor discovery entry in MQTTHaDiscovery.cpp.
2. Remove the publish if pressure diagnostics are already covered by existing flat scalar topics.
3. Convert fields to flat scalar topics (ADR-101 approach).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Either: MQTTHaDiscovery.cpp has a json_attributes_topic entry binding sat/pressure_health_attr to the sat/pressure_health entity
- [ ] #2 Or: the publish block at SATcontrol.ino:2042-2062 is deleted
- [ ] #3 No orphaned MQTT publish on sat/pressure_health_attr remains
- [ ] #4 Build passes and evaluator shows no new failures
<!-- AC:END -->
