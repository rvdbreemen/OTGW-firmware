---
id: TASK-1374
title: >-
  SAT: Fireplace mode switch - heat on the heating curve only, as with a missing
  room sensor (entry 49)
status: To Do
assignee: []
created_date: '2026-10-25T06:32:00Z'
labels:
  - sat
  - webui
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 509000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 06:18 UTC (#alpha-testing 1563799007157817405): 'Create a switch called Fireplace mode. When this switch is enabled SAT will send only the heating curve value to the control loop and not the PID. The behavior of SAT will be identical to the missing indoor temperature sensor we discussed earlier. Some homes have a stove or an AC heating their primary room [...] This new development will let SAT continue heating the home regardless of the main room reached the setpoint or not.' Reuse TASK-1364's curve-only path (satPidCurveOnly, P/I/D 0 per entry 46): setting SATfireplace (default off), REST/MQTT/HA switch, v2 toggle; while on, the room temperature is treated as not usable for control. Not in thermo-nova (grep fireplace/stove: nothing).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With Fireplace mode on SAT heats on the curve only with P/I/D 0 (host test red without the switch, green); switch on REST/MQTT/HA/v2 (browser test); off restores PID control
<!-- AC:END -->
