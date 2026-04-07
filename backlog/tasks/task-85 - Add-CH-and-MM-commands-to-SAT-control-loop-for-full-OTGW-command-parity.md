---
id: TASK-85
title: Add CH and MM commands to SAT control loop for full OTGW command parity
status: Done
assignee:
  - '@claude'
created_date: '2026-04-07 05:59'
updated_date: '2026-04-07 16:27'
labels:
  - sat-control
  - critical
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT control loop currently only sends CS=X (control setpoint) to the OTGW gateway. SAT Python sends THREE commands every control cycle: CS (setpoint), MM (max relative modulation), and CH (central heating enable). Without CH=1 the boiler may not start heating; without CH=0 it may not stop. Without MM control, PWM mode cannot suppress modulation properly. This task adds CH and MM command handling to match SAT Python heating_control.py lines 236-238.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every control cycle sends CH=1 when heating (setpoint > SAT_MIN_SETPOINT) and CH=0 when idle
- [x] #2 PWM enabled: send MM=0 (minimum modulation, duty cycle controls heat output)
- [x] #3 PWM disabled / continuous mode: send MM=<max_modulation> (user configured setting)
- [x] #4 DHW active: send MM=<max_modulation> (no suppression during hot water)
- [x] #5 When not heating (idle/summer/valves closed): send CS=10, MM=<max_modulation>, CH=0
- [x] #6 Auto-switch continuous->PWM threshold changed from 300s to 180s (3 minutes)
- [x] #7 Modulation suppression logic moved from continuous mode to PWM ON time only
- [x] #8 Comfort humidity offset gated behind settings.sat.bSummerSimmer enable flag
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. satApplyPWM(): add _pwm_flameOffHoldSetpoint static var; implement 4-step PWM ON CS sequence:\n   - Waiting for flame: CS = Tret + fFlameOffOffset, save as _pwm_flameOffHoldSetpoint\n   - Flame on, within fModSupDelay: CS = _pwm_flameOffHoldSetpoint (hold)\n   - Flame on, after fModSupDelay: CS = Tboiler - fModSupOffset\n   - PWM OFF: CS = SAT_MIN_SETPOINT, clear _pwm_flameOffHoldSetpoint\n2. Fix MM=0 for all PWM mode: line ~2948 change '!bPwmFlameRequested' to just SAT_MODE_PWM\n3. Remove continuous mode modulation suppression block (lines ~2909-2938); bModSuppressed no longer set in continuous mode\n4. Auto-switch 300s -> 180s: line ~2886 change 300000UL to 180000UL\n5. Add CH commands in hasOTCommandInterface() block: CH=1 when finalSetpoint > SAT_MIN_SETPOINT, CH=0 otherwise\n6. Early exits (summer, valves, thermal fallback): add CH=0 + MM=max alongside existing CS=10\n7. satUpdateComfort(): gate comfort humidity behind settings.sat.bSummerSimmer\n8. Test: verify all 8 ACs in code review
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
All 8 changes implemented in SATcontrol.ino:
- Change 1: satApplyPWM() - 4-step PWM ON CS sequence (Step2: Tret+offset, Step3: hold, Step4: Tboiler-offset)
- Change 2: Auto-switch 300s->180s (3 minutes)
- Change 3: Removed continuous mode modulation suppression block
- Change 4: MM=0 for all PWM mode (not just OFF phase)
- Change 5: CH=1/CH=0 commands added to main send block
- Change 6: Early exits (summer/valves/thermal fallback) now send MM=max + CH=0/CH=1
- Change 7: Comfort humidity gated by bSummerSimmer
- evaluate.py --quick: 15 pre-existing PROGMEM violations (unchanged from baseline), no new issues
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added CH/MM commands and PWM CS startup sequence to SAT control loop for full OTGW command parity with SAT Python.

Changes:
- satApplyPWM(): Added _pwm_flameOffHoldSetpoint static var; implemented 4-step PWM ON CS sequence: (1) waiting for flame: CS=Tret+offset to avoid ignition overshoot, (2) flame on within fModSupDelay: hold the low ignition setpoint, (3) flame stable: CS=Tboiler-fModSupOffset for steady-state, (4) OFF phase: CS=SAT_MIN_SETPOINT
- Auto-switch continuous->PWM threshold reduced from 300s (5min) to 180s (3min) for faster response
- Removed continuous mode modulation suppression block; modulation suppression now handled via CS sequence in satApplyPWM() only
- MM=0 now applies to all PWM mode (not just when flame not requested), matching SAT Python behavior where MM controls duty cycle indirectly
- CH=1/CH=0 added to main send block every cycle based on whether finalSetpoint > SAT_MIN_SETPOINT
- All early exits (summer simmer, valves closed, thermal fallback) now send MM=max + CH=0 (idle paths) or CH=1 (thermal fallback)
- satUpdateComfort(): comfort humidity offset now gated behind settings.sat.bSummerSimmer enable flag

All PROGMEM constraints satisfied (PSTR/F() used); no new violations vs baseline.
All 8 acceptance criteria verified.
<!-- SECTION:FINAL_SUMMARY:END -->
