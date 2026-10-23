---
id: TASK-1316
title: >-
  SAT PWM: send CH=0 with CS=10 when the PWM goes OFF, as thermo-nova clears the
  CH enable bit
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T10:32:00Z'
updated_date: '2026-10-23T13:18:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Sergeant D 2026-10-23: CH off below 28.2 (not thermo-nova's COLD_SETPOINT 22, const.py:34), otherwise as thermo-nova (heating_control.py:236-238: setpoint, max modulation, then heater state; coordinator/serial/__init__.py:210-212 maps it to the CH enable bit).
Code today (SATcontrol.ino): every SAT CS path already sends CH: main block 5019-5022 (CH = finalSetpoint > SAT_MIN_SETPOINT 10), thermal-safe 4779-4781 (CS 45, CH=1), summer 4809-4811 and valves 4822-4824 (CS 10, CH=0); the 3 s suppression refresh 953 runs in the ON phase only (CS above the threshold). A PWM OFF tick returns SAT_MIN_SETPOINT from satApplyPWM (913), so the main block already queues CS=10 + CH=0: Sergeant D's observation is not explained by the code -> ask which board and for a capture (on OT-Direct gateway mode CH=0 may not reach the boiler, TASK-1238; on PIC, CH= sets CHModeOff, gateway.asm:53/4372).
Change: new pure helper satChEnableFor(setpoint) in SATchEnable.h: 1 when setpoint >= SAT_CH_ENABLE_MIN_SETPOINT (28.2f), else 0 ('below 28.2' -> 28.2 itself is ON; recorded as interpretation). Main block uses it. Thermal-safe (45) and the CS=10 paths stay consistent (asserted by a source check). Write-on-change + 5 min refresh (TASK-565) kept; thermo-nova re-sends every step -> named to Sergeant D as an open point, not changed silently.
Verification: host test tests/test_sat_ch_enable.cpp on the real header (10, 22, 25, 28.1, 28.2, 45: old rule would give CH=1 for 22/25/28.1); source check that no path compares finalSetpoint > SAT_MIN_SETPOINT for CH any more (red on HEAD); build --target all not needed (no HAS_*) -> build.bat combo; evaluate; bench telnet on OTGW32: SAT on with a setpoint between 10 and 28.2 -> CS + CH=0 queued (old: CH=1). AC3 needs Sergeant D.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 12:49Z Sergeant D answer (#alpha-testing msg 1563172561162862714): 'below 28.2 not 22. Other than that follow the thermo-nova exactly'. So: CH enable OFF when the control setpoint sent is below 28.2 (not thermo-nova's COLD_SETPOINT 22), otherwise follow thermo-nova's sequence exactly (setpoint then CH enable after every control step, heating_control.py:238 / coordinator/serial/__init__.py:212). Unblocked.

2026-10-23 13:25 self-review + advisor: (1) the PWM OFF path is ALREADY correct at HEAD: satApplyPWM returns SAT_MIN_SETPOINT (SATcontrol.ino:913) and the main block queues CS then CH=0 from that setpoint (5019-5022); my 10:32 Discord message was wrong on this and was corrected (msg 1563179730084171818). AC2's 'old: CS=10 only' premise is false; the real red/green is the threshold (e.g. setpoint 25: HEAD CH=1, fix CH=0). (2) 28.2 = existing SAT_COLD_SETPOINT_RAD (SATcontrol.ino:55, TASK-891.2); underfloor 21.0 (:56) -> likely means satGetColdSetpoint(); asked. (3) Risk asked to Sergeant D: PWM ON hold (return+offset) can be below 28.2 on a cold start -> CH=0 would block ignition; does the rule apply in the ON phase? (4) Decide CH on the quantized sent value (lroundf(sp*10) vs 282), not the raw float. (5) Interaction with TASK-1270 (CH=0 window after enable). (6) Write-on-change kept, named to him. Waiting for Sergeant D's answer before implementing (SAT control behaviour).
<!-- SECTION:NOTES:END -->
