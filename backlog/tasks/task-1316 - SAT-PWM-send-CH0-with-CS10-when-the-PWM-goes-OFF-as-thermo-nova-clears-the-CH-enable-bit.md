---
id: TASK-1316
title: >-
  SAT PWM: send CH=0 with CS=10 when the PWM goes OFF, as thermo-nova clears the
  CH enable bit
status: To Do
assignee: []
created_date: '2026-10-23T10:32:00Z'
labels:
  - sat
  - pwm
  - opentherm
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 439000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #alpha-testing 2026-10-23 10:21 UTC (msg 1563135315991072789): 'the CS is following the BT correctly now ... When the PWM goes to OFF, correctly you send the CS=10 command, but you should also send the command CH=0.'
thermo-nova @7beaaf5 (verified against GitHub raw at the pinned commit, identical to the local copy): after every control step heating_control.py:236-238 sends the control setpoint, the max modulation, then async_set_heater_state(HeaterState.ON if self._control_setpoint > COLD_SETPOINT else HeaterState.OFF); const.py:34 COLD_SETPOINT = 22; the serial (OTGW) coordinator maps it to the CH enable bit: coordinator/serial/__init__.py:210-214 set_ch_enable_bit(1 if state == HeaterState.ON else 0).
Firmware today: the main control block sends CH from finalSetpoint > SAT_MIN_SETPOINT (10) (SATcontrol.ino ~5019-5022, satEnqueueIfChangedCH ~430); the PWM OFF phase sends CS=10 through its own path (to be located, likely the 3 s PWM refresh) without CH=0. Differences to settle with the code: (a) CH=0 on the PWM OFF path (Sergeant D's point); (b) the threshold: thermo-nova clears CH whenever the control setpoint is at or below COLD_SETPOINT 22, the firmware at or below 10.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every path that sends CS for SAT also sends the CH state thermo-nova would send (CH=0 when the control setpoint <= COLD_SETPOINT, CH=1 above), with file:line citations of both; the threshold choice (22 vs 10) confirmed by Sergeant D and recorded
- [ ] #2 Replay or host test old vs fix: a PWM ON->OFF transition sends CS=10 and CH=0 (old: CS=10 only), and ON again sends CH=1
- [ ] #3 Sergeant D confirms on his setup
<!-- AC:END -->
