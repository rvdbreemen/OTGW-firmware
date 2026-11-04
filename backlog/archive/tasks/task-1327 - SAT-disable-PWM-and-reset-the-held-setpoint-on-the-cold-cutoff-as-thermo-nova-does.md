---
id: TASK-1327
title: >-
  SAT: disable PWM and reset the held setpoint on the cold cutoff, as
  thermo-nova does
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T17:53:00Z'
updated_date: '2026-10-23T19:55:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 450000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1323 review (opus), pre-existing, now hit more often because the solar offset can keep the requested setpoint at or below the cold setpoint for hours: thermo-nova calls self._pwm.disable() and sets _control_setpoint = MINIMUM_SETPOINT when the heater state is OFF (heating_control.py:229-234; pwm.py:106-109 disable resets PWM). Our firmware stays in SAT_MODE_PWM, satPwmSchedule keeps its phase timers running on cold ticks, and _sat_lastControlCS keeps the pre-cutoff value (SATcontrol.ino ~5003 vs the cutoff ~5030). Scenario: forced PWM, overshoot protection off, solar keeps requested at 27 (CS=10, CH=0); when solar clears to 30, satApplyPWM returns the stale prevCS (e.g. 45) with CH=1, where thermo-nova would start from a reset PWM. Needs Sergeant D's OK (SAT control).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D's choice recorded (mirror pwm.disable() and/or reset the held CS on the cutoff), with thermo-nova citations
- [x] #2 Host test: cutoff then recovery no longer returns the stale held CS (red before, green after)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved by Sergeant D 2026-10-23 18:13Z (docs/sat/sergeant-d-corrections.md entry 12; relies on entry 7 per-system cold setpoint and entry 11 cutoff '<=' on the requested setpoint incl. solar). thermo-nova: heater state OFF (requested <= COLD) -> self._pwm.disable(), self._reset_pwm_disable_guards(), _control_setpoint = MINIMUM_SETPOINT (heating_control.py:229-234); disable() = reset + enabled False (pwm.py:106-109); PWM re-enables through the normal enable path afterwards (trace and cite). Firmware today: cold cutoff (SATcontrol.ino ~5030) forces CS=10/CH=0 but stays in SAT_MODE_PWM, satPwmSchedule keeps its phase timers, _sat_lastControlCS keeps the pre-cutoff value (~5003) -> stale prevCS after recovery. Change: on the cutoff tick mirror pwm.disable(): satPwmReset() (phase, ignition wait, hold) and set _sat_lastControlCS = SAT_MIN_SETPOINT; map thermo-nova's 'PWM enabled' flag to our modes exactly as the trace shows (bForcePWM respected), without touching other paths. Tests: test_sat_pwm_override.py tick test: PWM ON with a held CS 45, then a cutoff tick, then recovery to requested 30 -> no stale 45 (red on HEAD), PWM phase reset; CH/CS consistent. Build, evaluate, bench boot check. Review incl. a Sergeant D entries lens.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 18:13Z Sergeant D (msg 1563254104346730600): 'yes keep the thermo-nova solution' -> approved: on the cold cutoff disable PWM (pwm.disable(), pwm.py:106-109) and reset the control setpoint to MINIMUM (heating_control.py:229-234).

2026-10-23 22:00 evidence: satPwmDisableOnHeaterOff() (SATcontrol.ino) mirrors thermo-nova's heater-OFF branch (heating_control.py:229-234; pwm.py:106-109): satPwmReset(), satCycleResetSustainGuards() (new, SATcycles.ino; :231/:525-528), _sat_lastControlCS = SAT_MIN_SETPOINT, iCurrentModulation = iMaxRelModulation (:234), and leaves SAT_MODE_PWM unless bForcePWM (thermo-nova control_mode = CONTINUOUS while PWM disabled, :148-152; force PWM has no thermo-nova counterpart, entry_data.py:223 only; asked Sergeant D msg 1563276791723073720, and with bForcePWM the loop forces PWM back on the next tick anyway). Called on the cold cutoff and on all valves closed (:69-70). Not covered: summer simmer; thermo-nova's on_cycle_end re-enable (pwm.py:219-223) -> TASK-1331. Review (opus, incl. the Sergeant D entries lens) found the missing guard reset (conflict with entry 12) and the valves path; both fixed. Tests: test_sat_pwm_override.py fix 'Ran 60 OK', HEAD 'FAILED (failures=8)' (stale CS 45 vs 30, guards 64->0, MM 55 vs 80, mode 2 vs 1, valves tuple); test_sat_ch_enable 0 failures; evaluate 0 fail; build.bat combo SUCCESS 21:52; bench app flash: SAT active, no crashlog (no room temp on the bench, control path host-tested only).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT now switches PWM off the thermo-nova way (Sergeant D 2026-10-23, entry 12) when the requested setpoint drops to the cold setpoint or below, and when all valves close: it resets the PWM phase, the overshoot/underheat/saturation guards and the remembered setpoint (10), sends the maximum relative modulation and leaves PWM mode unless Force PWM is on (open question to Sergeant D; Force PWM has no thermo-nova counterpart). Evidence: AC1 decision recorded (entry 12) with citations; AC2 host tests of the real helper red on HEAD (stale held CS 45 after recovery, guards kept, MM 55) and green on the fix. Not verified on a boiler. Not ported: on_cycle_end re-enable (TASK-1331).
<!-- SECTION:FINAL_SUMMARY:END -->
