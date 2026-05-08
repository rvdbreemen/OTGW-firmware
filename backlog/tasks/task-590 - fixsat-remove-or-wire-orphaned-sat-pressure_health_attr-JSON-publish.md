---
id: TASK-590
title: 'fix(sat): remove or wire orphaned sat/pressure_health_attr JSON publish'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 17:13'
updated_date: '2026-05-08 21:30'
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
- [x] #2 Or: the publish block at SATcontrol.ino:2042-2062 is deleted
- [x] #3 No orphaned MQTT publish on sat/pressure_health_attr remains
- [x] #4 Build passes and evaluator shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Remove the sat/pressure_health_attr publish block (lines 1888-1908 in SATcontrol.ino). The flat scalar topics (sat/pressure, sat/pressure_drop_rate, sat/pressure_alarm) already cover all the data. No HA discovery entry exists for sat/pressure_health itself, so wiring option would require a larger change out of scope. Removing the orphan is the minimal fix.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the orphaned sat/pressure_health_attr publish block from satPublishMQTT().\n\nRationale: no HA discovery entry existed for sat/pressure_health (the binary sensor topic), so sat/pressure_health_attr was consumed by nobody. Wiring would require a new SAT binary sensor discovery path out of scope for this task. The flat scalar topics already cover the pressure data.\n\nChanges:\n- Deleted 20-line pressAttrBuf block (static char[200] + JSON build + sendMQTTData call).\n- Version bumped beta.2 -> beta.3.\n\nBuild: pass. No new evaluator failures.\nPushed: origin/dev a1a7795e.
<!-- SECTION:FINAL_SUMMARY:END -->
