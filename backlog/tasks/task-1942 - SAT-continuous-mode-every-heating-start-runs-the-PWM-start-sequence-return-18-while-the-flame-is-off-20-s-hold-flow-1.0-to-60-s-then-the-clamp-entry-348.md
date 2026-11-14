---
id: TASK-1942
title: >-
  SAT continuous mode: every heating start runs the PWM start sequence (return +
  18 while the flame is off, 20 s hold, flow - 1.0 to 60 s, then the clamp)
  (entry 348)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T17:23:00Z'
updated_date: '2026-11-14T18:04:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 994000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 348 (Q-347a B, 2026-11-14, 'as a fail safe guard (e.g. sat is in PWM -> reboot -> starts again in suppression)'): a heating start in continuous mode runs the PWM start sequence too. Follow-up of TASK-1924 Part 2 (beta.250, entries 338/344/347). Deliberate deviation from thermo-nova: there the start sequence is PWM-only (heating_control.py:224-225, :380-448) and continuous mode with the flame off sends the request (:476-482). Open in the entry: the return-less case of entry 45 also in continuous (read as yes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In continuous mode with heating requested and the flame off, CS = return + fFlameOffOffset (18.0), clamped to cold setpoint..max (satPwmFlameOffHold, as PWM); a boiler without a return temperature uses the flow, as PWM (entry 45, read as yes)
- [x] #2 After a CH flame-on the flame-off CS is held for fModSupDelay (20 s); then CS = max(request, flow - fModSupOffset) until 60 s after flame-on; then the entry-112 clamp (entries 344, 347)
- [x] #3 Gates as the PWM ON start: no override during hot water, with the request at or below the cold threshold, or with overshoot protection off (heating_control.py:384-389)
- [x] #4 Host test on the real satContinuousSetpoint/satPwmFlameOffHold: flame off then flame on, red on beta.250 (flame off -> request), green on the fix
- [ ] #5 Bench via 09: a continuous start shows return + 18 before ignition, the 20 s hold, then the follow and the clamp; no regression on PWM starts
- [x] #6 ADR-235 (or the TASK-1924 docs) names the start sequence in continuous mode
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries relied on: 45, 112, 337, 338, 344, 347, 348 (agree with all; 348 is a deliberate deviation from thermo-nova, decided by Sergeant D). Thermo-nova (7beaaf5, opened 2026-11-14): start sequence PWM-only (heating_control.py:224-225 calls _compute_pwm_control_setpoint only in PWM); its gates :384-389 (hot water, request <= COLD_SETPOINT, overshoot protection off -> no override); flame off -> return + flame_off_setpoint_offset 18 (:392-405, const.py:151); flame on, < modulation_suppression_delay 20 s (const.py:152) -> hold the flame-off value, or pending if none (:414-433); then flow - 1.0 (:439-448, const.py:153). Continuous with flame off -> request (:476-482). Firmware today (beta.250): satContinuousSetpoint() = 60 s follow max(request, flow - fModSupOffset) after a CH flame-on edge, else satApplyContinuous() (flame off -> request). PLAN: (1) satContinuousSetpoint() gets the PWM start phases, with the PWM ON gates (DHW, request <= cold threshold, overshoot protection off -> today's behaviour): flame off -> satPwmFlameOffHold() (return + fFlameOffOffset, flow for a no-return boiler, clamped cold..max), kept in its own _sat_contFlameOffHold; flame on and < fModSupDelay since the CH flame-on edge -> that hold (no-return: live flow + offset, as PWM), or the previous CS if none was recorded (pending); then to 60 s max(request, flow - fModSupOffset); then the entry-112 clamp. Return unknown at boot and not yet a no-return boiler -> today's request (as PWM keeps prev). (2) Entry 348 open point 1 read as yes: satNoReturnFloodAllowed() also allows continuous mode; the flood hands over to satContinuousSetpoint() there (PWM: suppression, unchanged); the 5 s refresh skips while the flood runs. (3) Tests: tests/test_sat_continuous_startup.py on the real satPwmFlameOffHold + satContinuousSetpoint: flame off then on, red on beta.250 (flame off -> request), green; no-return, protection off, DHW cases; structural flood gate. Existing SAT suite in chunks. (4) Bump, evaluate, build after coordinator GO, bench via 09 (continuous start with return + 18 before ignition). (5) ADR-235 text. Risk named: in continuous a flame-off CS of return + 18 can sit above the request, so a boiler idling on its own hysteresis relights to a higher setpoint; this is the PWM behaviour Sergeant D chose to extend; bench and field watch starts.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (d3, 2026-11-14, local, beta.252, not yet built). satContinuousSetpoint(): PWM ON gates (overshoot protection on, no DHW, request above the cold threshold) -> flame off: satPwmFlameOffHold() (return + 18 / flow for a no-return boiler, clamped cold..max) in _sat_contFlameOffHold; flame on < fModSupDelay since the CH flame-on edge: that hold (no-return: live flow + 18), or the previous CS if none (pending); then to 60 s max(request, flow - fModSupOffset); then the entry-112 clamp (fFlowOffset 2.0, entry 349). Return unknown at boot and not a no-return boiler: the request, as before. Flood (entry 348 point 1, read as yes): satNoReturnFloodAllowed() also in continuous mode, hand-over to satContinuousSetpoint(); the 5 s refresh skips while it runs. FINDING fixed with it: satControlLoopRun() returns before the flame-edge code while SAT is off, so switching SAT on with a burning boiler gave a fake CH flame-on edge; in continuous mode that would have started the flood (10x PM=25 + flow + 18 on a running boiler). New _sat_flameOffSeen (false while SAT is off, true once the flame is seen off): a CH start (window and continuous flood) needs it. The cycle tracker keeps its edge (unchanged). Caught by test_sat_no_return_startup::test_runtime_enable_with_a_running_boiler_sends_no_flood... (red on the first version of this change, green with the guard). Tests: tests/test_sat_continuous_startup.py on the real satPwmFlameOffHold + satContinuousSetpoint, 11/11 on the fix; on beta.250 (baf84d9) 3 FAIL: start with return 30 -> 50.0 (request) instead of 48.0, no-return 50.0 vs 48.0, flood gate absent. Harnesses updated for the new symbols: test_sat_no_return_ignition, test_sat_no_return_startup, test_sat_ot_loss, test_sat_restart_quiet, test_sat_ceiling_paths (flood hand-over anchor). Runs: SAT a-o 45/45, p-s 36/36, t-9+l + OTD + adr_governance 50/50, evaluate --quick exit 0. ADR-235 names the continuous start sequence and entries 348/349 (AC6). AC5 (bench) open.
<!-- SECTION:NOTES:END -->
