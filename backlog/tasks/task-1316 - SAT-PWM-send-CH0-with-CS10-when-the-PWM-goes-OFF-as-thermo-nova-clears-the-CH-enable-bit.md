---
id: TASK-1316
title: >-
  SAT PWM: send CH=0 with CS=10 when the PWM goes OFF, as thermo-nova clears the
  CH enable bit
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T10:32:00Z'
updated_date: '2026-10-23T16:06:00Z'
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
- [x] #1 Every path that sends CS for SAT also sends the CH state thermo-nova would send (CH=0 when the control setpoint <= COLD_SETPOINT, CH=1 above), with file:line citations of both; the threshold choice (22 vs 10) confirmed by Sergeant D and recorded
- [x] #2 Replay or host test old vs fix: a PWM ON->OFF transition sends CS=10 and CH=0 (old: CS=10 only), and ON again sends CH=1
- [ ] #3 Sergeant D confirms on his setup
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Implement per Sergeant D 2026-10-23: (1) pure helper SATchEnable.h satChEnableFor(setpoint, coldSetpoint): 1 when lroundf(setpoint*10) >= lroundf(coldSetpoint*10) (quantized like satEnqueueIfChangedCS / CS=%.1f), else 0; main block SATcontrol.ino ~5021 uses satChEnableFor(finalSetpoint, satGetColdSetpoint()) instead of finalSetpoint > SAT_MIN_SETPOINT. (2) satApplyPWM flame-off hold floored at satGetColdSetpoint() (return+offset never below it). (3) Other CS paths unchanged and consistent (thermal-safe 45 -> CH=1; summer/valves 10 -> CH=0). Tests: tests/test_sat_ch_enable.cpp on the real header (rad: 10,22,25,28.1,28.15,28.17,28.2,28.25,45; floor: 20.9,21.0,25) red/green vs the old rule; tests/test_sat_pwm_override.py gains a cold-start case (return 18 + offset < 28.2 -> hold = 28.2 rad / 21 floor; red on HEAD); a source check that the main block uses the helper. build.bat combo, evaluate. Bench: OTGW32 with SAT on: a requested setpoint between 21 and 28.2 on radiators -> CS + CH=0 queued (HEAD: CH=1). AC3 Sergeant D.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 12:49Z Sergeant D answer (#alpha-testing msg 1563172561162862714): 'below 28.2 not 22. Other than that follow the thermo-nova exactly'. So: CH enable OFF when the control setpoint sent is below 28.2 (not thermo-nova's COLD_SETPOINT 22), otherwise follow thermo-nova's sequence exactly (setpoint then CH enable after every control step, heating_control.py:238 / coordinator/serial/__init__.py:212). Unblocked.

2026-10-23 13:25 self-review + advisor: (1) the PWM OFF path is ALREADY correct at HEAD: satApplyPWM returns SAT_MIN_SETPOINT (SATcontrol.ino:913) and the main block queues CS then CH=0 from that setpoint (5019-5022); my 10:32 Discord message was wrong on this and was corrected (msg 1563179730084171818). AC2's 'old: CS=10 only' premise is false; the real red/green is the threshold (e.g. setpoint 25: HEAD CH=1, fix CH=0). (2) 28.2 = existing SAT_COLD_SETPOINT_RAD (SATcontrol.ino:55, TASK-891.2); underfloor 21.0 (:56) -> likely means satGetColdSetpoint(); asked. (3) Risk asked to Sergeant D: PWM ON hold (return+offset) can be below 28.2 on a cold start -> CH=0 would block ignition; does the rule apply in the ON phase? (4) Decide CH on the quantized sent value (lroundf(sp*10) vs 282), not the raw float. (5) Interaction with TASK-1270 (CH=0 window after enable). (6) Write-on-change kept, named to him. Waiting for Sergeant D's answer before implementing (SAT control behaviour).

2026-10-23 13:31Z Sergeant D (msg 1563182969345282071): CH threshold = the per-system cold setpoint ('You are correct': 28.2 radiators SATcontrol.ino:55, 21 underfloor :56, via satGetColdSetpoint()). PWM ON cold start: 'We can limit the cold start value to 28.2 and 21 for underfloor' -> floor the flame-off hold setpoint (satPwmFlameOffHold, return+offset) at satGetColdSetpoint() so CH stays on during ignition. Deviation from thermo-nova (COLD_SETPOINT 22, const.py:34; no hold floor) decided by Sergeant D. No answer on board/capture and on write-on-change: write-on-change + 5 min refresh kept (no objection, not an explicit OK).

2026-10-23 15:55 implementation (uncommitted, stashed while waiting): SATchEnable.h satChEnableFor() (quantized); main block uses it with satGetColdSetpoint(). Hold floor at the cold setpoint already existed at HEAD (satPwmFlameOffHold, TASK-891.2, 73295292b) -> comment only. Evidence this session: test_sat_ch_enable.cpp 0 failures (old rule CH=1 for 22/25/28.1 and underfloor 20.9, new CH=0); test_sat_pwm_override.py fix 'Ran 39 OK', HEAD 'FAILED (failures=1)' = the source check (cold-start hold test passes on both: regression guard). evaluate --quick 0 fail. Open, asked Sergeant D (msg 1563187099484557367): (1) solar offset after the hold can push the sent setpoint below the cold setpoint -> CH=0 during ignition; decide CH before the solar offset? (2) PWM ON suppression flow-offset below the cold setpoint -> CH=0 mid-burn on the next control step (thermo-nova same mechanism, heating_control.py:236-238, but with 22 rarely hit): skip the CH rule in PWM ON, or floor the suppression setpoint too?

Parked in git stash on dev: 'TASK-1316 CH threshold (waiting for Sergeant D)' (tracked edits) and 'TASK-1316 new files (SATchEnable.h, test_sat_ch_enable.cpp)'. Restore both with git stash pop (newest first) when resuming.

2026-10-23 14:51-14:54Z Sergeant D (msgs 1563203205633020027, ...729652457585, ...4066262130719): 'The CS we send to the boiler is decoupled to the duty cycle calculation. The CS that we send during PWM just keeps the flame on at the minimum modulation level. The hard job of the cycle duration is in the pwm.py file'; heating starts if (requested setpoint - base temperature) / (BT - base temperature) is suitable; duty percentage translated to seconds on/off. Not an explicit answer to the two questions. Proposed rule sent for a yes/no (msg 1563205445781422121): PWM ON -> CH=1 regardless of CS; PWM OFF -> CS=10 + CH=0; continuous -> CH from the sent setpoint vs the cold setpoint; CH decided before the solar offset. Waiting.

2026-10-23 15:08Z Sergeant D (msg 1563207530275152037), reply to the 4-point CH proposal: 'great understanding! As you understand the continuous and pwm buttons should never be present in the SAT dashboard. Because the software decides when every mode will be in charge.' Read as YES to the proposal (interpretation recorded): PWM ON -> CH=1; PWM OFF -> CS=10 + CH=0; continuous -> CH from the setpoint vs the cold setpoint; CH decided on the setpoint before the solar offset. The buttons remark is filed separately.

2026-10-23 18:05 implementation of the confirmed rule: SATchEnable.h satChEnableDecide(preSolar, cold, pwmDrivesSetpoint, pwmOn, coldCutoff) + satChEnableFor; SATcontrol.ino main block: satPwmDrivesSetpoint() mirrors satApplyPWM's early returns (flow<=0, protection off, DHW, pid<=cold), preSolarSetpoint captured before the solar offset, pwmOn captured right after satApplyPWM (opus review HIGH: reading bPwmFlameRequested after satCycleCheckAutoSwitch would send CH=0 mid-burn on a PWM->continuous switch tick, because satPwmReset clears it). Evidence: test_sat_ch_enable.cpp 0 failures, 22 PASS, per case old rule vs new (continuous 25 rad old 1 new 0; underfloor 20.9 old 1 new 0; cold cutoff with pwmOn old 1 new 0; PWM not driving CS + pre 25 old 1 new 0); test_sat_pwm_override.py fix 'Ran 39 OK', HEAD 'FAILED (failures=1)' (source check incl. the pwmOn-before-auto-switch order guard); evaluate 0 fail; build.bat combo SUCCESS 18:02; bench app flash: boots, SAT active, no panic in 120 s (no CS/CH traffic because the bench has no room temperature, so the CH path itself is host-tested only). Follow-ups: TASK-1323 (sticky prevCS + solar during ignition, needs Sergeant D), TASK-1324 (write-on-change resend gap, pre-existing). AC3 open: Sergeant D on his setup.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT now sets the CH enable bit per Sergeant D's confirmed rule (2026-10-23): PWM ON keeps CH on whatever CS, PWM OFF sends CS=10 + CH=0, otherwise CH is off below the per-system cold setpoint (28.2 radiators / 21 underfloor, quantized like the CS command), decided on the setpoint before the solar offset; the cold cutoff forces CH=0. Before, CH followed 'setpoint > 10'. AC1: rule recorded with thermo-nova citations and Sergeant D's answers; AC2: host test old vs new per case + source check red on HEAD (the PWM OFF path already sent CH=0 at HEAD; the red/green is the threshold and the PWM ON rule). Open (AC3): Sergeant D confirming on his setup. Not verified on a device (bench has no room temperature, SAT returns before the CH step).
<!-- SECTION:FINAL_SUMMARY:END -->
