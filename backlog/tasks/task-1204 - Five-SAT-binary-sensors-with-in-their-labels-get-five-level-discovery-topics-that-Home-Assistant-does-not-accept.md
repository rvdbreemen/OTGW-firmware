---
id: TASK-1204
title: >-
  Five SAT binary sensors with '/' in their labels get five-level discovery
  topics that Home Assistant does not accept
status: To Do
assignee: []
created_date: '2026-10-03 17:54'
labels:
  - ha-discovery
  - bug
  - sat
dependencies: []
priority: medium
ordinal: 326000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by a TASK-1201 verifier (code reading, not yet observed in Home Assistant; SAT is off on the bench). buildBinSensorDiscoveryTopic() (MQTTHaDiscovery.cpp:2659-2676) does not call sanitizeHaObjectId(), while buildSensorDiscoveryTopic() does (:2636). The binary rows sat/weather/is_day, sat/active, sat/safety_tripped, sat/flame_health and sat/valves_open (near :1448-1453) therefore produce topics such as homeassistant/binary_sensor/<node>/sat_sat/active/config, one level deeper than Home Assistant's discovery pattern. TASK-872 sanitized only the binary uniq_id (:2510).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on the laptop rig with SAT enabled: the five configs land on topics with an extra level, and Home Assistant creates no entity for them
- [ ] #2 Binary-sensor topics are sanitized like sensor topics; the uniq_id and state topic of each entity stay unchanged
- [ ] #3 On the rig, Home Assistant shows the five SAT binary sensors after a republish
- [ ] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
