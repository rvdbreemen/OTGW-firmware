---
id: TASK-3032
title: 'SAT: a thermostat setpoint change that equals a preset selects that preset'
status: To Do
assignee: []
created_date: '2026-11-22T09:14:00Z'
updated_date: '2026-11-22T09:22:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1141000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: entry 465 in the private SAT register (Sergeant D, 2026-11-22, amends entry 25), from gruijter's request (#alpha-testing 1573989200657653801: his Homey app always shows preset 'none'). Today satAdoptThermostatSetpoint() (SATcontrol.ino:2445-2468) clears the preset and skips matching on purpose; the MQTT/web UI path matches within 0.05 C (satHandleTargetTemp, SATcontrol.ino:3895-3925). Change: on a thermostat-driven target change, if the value equals a preset (within 0.05 C) select that preset; otherwise keep today's behaviour (preset cleared). A later 'preset off' must not restore an old pre-preset temperature over the thermostat's value (fPreCustomTemp handling).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Thermostat change to a preset value selects that preset (MQTT sat/preset and HA climate show it)
- [ ] #2 Thermostat change to a non-preset value still clears the preset
- [ ] #3 'Preset off' after a thermostat-driven match keeps the thermostat value (no stale restore)
- [ ] #4 Host test on the real satAdoptThermostatSetpoint() red on the old code, green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22: Robert in #alpha-testing (1573992000523341935): put it in the backlog and prioritize for the 2.0.0 release.
<!-- SECTION:NOTES:END -->
