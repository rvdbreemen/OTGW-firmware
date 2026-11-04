---
id: TASK-99
title: 'SAT Audit B8: DHW (domestic hot water) control'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:50'
updated_date: '2026-04-09 05:21'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the DHW (Domestic Hot Water) control in Python SAT thermo-nova with C++ firmware. Verify DHW priority, setpoint management, comfort mode and interaction with heating control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DHW priority and switching compared
- [x] #2 DHW setpoint management verified
- [x] #3 DHW comfort mode behavior verified
- [x] #4 CH/DHW interaction (legionella, priority) verified
- [ ] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B8 audit: DHW domestic hot water control - GAPS FOUND

Python: DHW detection via coordinator hot_water_active flag. When DHW active: (1) overshoot-to-PWM guard is suppressed via DHW_OVERSHOOT_GUARD_SECONDS=300s; (2) PWM disable guards are suppressed; (3) relative_modulation_state returns HOT_WATER; (4) control setpoint is NOT modified (boiler manages DHW itself). Also DHW setpoint management: async_set_control_hot_water_setpoint() sends DHW setpoint via coordinator.

C++ (SATcontrol.ino satControlLoop()):
- DHW detection: OTcurrentSystemState.SlaveStatus bit 2 (0x04) = DHW active ✓
- When DHW active: return early, skip CH setpoint control ✓
- DHW setpoint: settings.sat.fDhwSetpoint published via MQTT but no active DHW setpoint sending via OT

GAPS:
1. Python actively sets DHW setpoint via coordinator when SAT controls it. C++ publishes fDhwSetpoint to MQTT but does NOT send a DHW setpoint command (TW=) to the boiler. The firmware reads OT data but never issues TW= (DHW setpoint override).
2. Python DHW_OVERSHOOT_GUARD_SECONDS=300: after DHW ends, suppresses overshoot-to-PWM switching for 300s. C++ has no equivalent post-DHW guard (confirmed in B1 analysis). Overshoot after DHW cycle will immediately trigger PWM switch.
3. Python tracks hot_water_off_since timestamp for the guard. C++ has no equivalent tracking.

Created audit-fix task for DHW setpoint command and post-DHW guard.
<!-- SECTION:FINAL_SUMMARY:END -->
