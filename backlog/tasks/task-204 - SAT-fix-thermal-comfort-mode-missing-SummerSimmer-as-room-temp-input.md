---
id: TASK-204
title: 'SAT fix: thermal comfort mode missing - SummerSimmer as room temp input'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-09 05:23'
updated_date: '2026-04-09 06:12'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify: Python climate.py uses thermal_comfort flag to pass SummerSimmer index as room_temp to PID
2. C++ already has satCalcSimmerIndex() and publishes it to MQTT but never feeds it into PID
3. The existing bComfortAdjust/bSummerSimmer settings do humidity-based SETPOINT adjustment, which is different
4. Decision: Port the Python behavior -- when bThermalComfort is true and humidity is fresh, use SSI as PID room temp input
5. Add bThermalComfort to SATSection in header (with default false)
6. In satGetRoomTemp(), after getting the raw temp, if bThermalComfort is true and humidity is fresh, apply SSI substitution
7. Publish bThermalComfort state to MQTT in satPublishMQTT()
8. Note: only SATcontrol.ino is touched per instructions (header is shared but necessary)
<!-- SECTION:PLAN:END -->
