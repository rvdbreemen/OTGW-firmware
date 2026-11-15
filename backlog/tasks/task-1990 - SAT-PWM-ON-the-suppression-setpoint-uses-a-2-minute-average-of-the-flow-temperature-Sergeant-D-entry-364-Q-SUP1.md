---
id: TASK-1990
title: >-
  SAT PWM ON: the suppression setpoint uses a 2 minute average of the flow
  temperature (Sergeant D entry 364, Q-SUP1)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-15T21:35:00Z'
updated_date: '2026-11-15T21:39:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1014000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D answered Q-SUP1 with 'A, 2 minutes window' (entry 364, https://discord.com/channels/812969634638725140/1105556725714649128/1571637957230862506): during PWM ON the modulation suppression setpoint is computed from an average flow temperature over a 2 minute window instead of the latest flow value. This deviates from thermo-nova, which uses the latest flow (heating_control.py:435-441, flow = boiler_temperature at coordinator/__init__.py:139, latest MsgID 25 at coordinator/mqtt/opentherm.py:107-110). Firmware today: CS = satGetFlowTemp() - fModSupOffset, refreshed every 3 s (SATcontrol.ino:1812 refresh, satPwmSuppressionSetpoint :1264). It replaces the directive choice B (latest value). Our reading of '2 minutes window' (moving average of the last 2 minutes of flow readings) is to be confirmed in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The average used (window 2 min, which samples, start-up when fewer than 2 min of samples exist, flame-off and DHW handling) is planned with citations and the reading of entry 364 is confirmed or corrected with Sergeant D
- [ ] #2 During PWM ON the suppression CS is the 2 minute average of the flow minus fModSupOffset; a host test on the real code is red on the old code and green on the fix
- [ ] #3 The start-up flood (entry 334b/Q-334b, 2 s) and the flow clamp (entries 336-349) keep their own flow source unless the plan says otherwise, with the entries named
- [ ] #4 Bench or simulation evidence of the CS during a PWM ON phase
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Owner: 95 (after TASK-1412.18), per the coordinator 2026-11-15.
<!-- SECTION:NOTES:END -->
