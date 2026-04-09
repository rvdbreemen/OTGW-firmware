---
id: TASK-204
title: 'SAT fix: thermal comfort mode missing - SummerSimmer as room temp input'
status: To Do
assignee: []
created_date: '2026-04-09 05:23'
labels:
  - audit-fix
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT has a 'thermal_comfort' config option that substitutes SummerSimmer.index(room_temp, humidity) as the room temperature input to the PID controller (climate.py:276-277). This means the control loop targets a heat-index-adjusted temperature rather than raw sensor temp. C++ firmware has the SummerSimmer index calculation but only publishes it as a sensor; it never substitutes it as room temperature in the PID control loop. Feature is missing entirely.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Verify Python thermal_comfort flag effect on PID control
- [ ] #2 Decide whether thermal comfort PID substitution should be ported to C++
- [ ] #3 If ported: add bThermalComfort setting and use SummerSimmer.index in satGetRoomTemp()
- [ ] #4 Publish thermal_comfort state to MQTT
<!-- AC:END -->
