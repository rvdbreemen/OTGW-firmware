---
id: TASK-1990
title: >-
  SAT PWM ON: the suppression setpoint uses a 2 minute average of the flow
  temperature (Sergeant D entry 364, Q-SUP1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T21:35:00Z'
updated_date: '2026-11-15T23:04:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Checked against entries 364 (this answer), 45/142/149/151 (start-up flood), 336-349 (continuous flow clamp), 230 (OFF phase), 267 (bModSuppressed). thermo-nova 7beaaf5, opened this session: heating_control.py:435-441 suppressed_setpoint = device_state.flow_temperature - modulation_suppression_offset_celsius; flow_temperature = boiler_temperature (coordinator/__init__.py:139), the latest MsgID 25 value (coordinator/mqtt/opentherm.py:107-110). Firmware today: satPwmSuppressionSetpoint() SATcontrol.ino:1264-1270 uses satGetFlowTemp() (latest fresh MsgID 25), called by the tick (satApplyPWM :1809), the 3 s refresh (:1830) and the flood hand-over (:1992).
Reading of '2 minutes window' to confirm with Sergeant D (one question, via the coordinator/92): A) a moving average of the valid MsgID 25 samples of the last 120 s, counting only samples since the current flame-on and none taken while the boiler DHW bit is set; with fewer than 120 s since flame-on it averages what it has (at the hand-over after fModSupDelay that is the samples since ignition). B) the same window over every valid sample, also from before the ignition (OFF phase, standing water). C) something else. Recommendation A: indigo_light's 2026-11-15 capture shows MsgID 25 climbing to 63 C with the flame off after every CH run (standing water / internal sensor), and B would average such readings into the first ON minutes. Under Robert's directive the work goes ahead on A; a different answer becomes a follow-up.
Implementation: new header SATflowAvg.h (no Arduino state, host-compilable like SATboilerFeed.h): a ring of (millis, value) samples, one stored at most every 2 s, 64 slots (512 B), mean over samples with now - t < 120000 ms; satFlowAvgNote() from the processOT() feed hook OTGW-Core.ino:6336 on a Valid MsgID 25 boiler sample; satFlowAvgReset() on every flame-on edge and when the DHW bit is set. satPwmSuppressionSetpoint() uses the average when it has at least one sample, else satGetFlowTemp() (same validity gate: no average without a fresh flow). Only the suppression uses it: the start-up flood (flow + offset every 2 s, entries 45/151) and the continuous follow/clamp (entries 336-349, satContinuousSetpoint) keep the latest flow (AC3); the flood hand-over :1992 calls satPwmSuppressionSetpoint() and so gets the average. Telemetry: the SAT trace line and sat?detail=full get the average and its sample count. ADR: a deliberate deviation from thermo-nova by Sergeant D's answer; a short ADR (guideline-level, its gate the host test). Tests: host test compiling SATflowAvg.h + the real satPwmSuppressionSetpoint(): a step in the flow inside the window gives the mean, not the last value (red on HEAD), samples older than 120 s drop out, reset on flame-on, the 2 s storage spacing; a SAT simulation run on the host for AC4. Bench is not available (Robert 2026-11-15): AC4 by simulation.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Owner: 95 (after TASK-1412.18), per the coordinator 2026-11-15.

Implemented (session 95, 2026-11-16 00:xx): SATflowAvg.h (ring of 64 (millis, value) samples, one per 2 s at most, mean over the last 120 s since a given flame-on time); OTBustypes.h state.otBus.flowAvg; OTGW-Core.ino processOT(): a separate block after the feed hook notes a valid boiler MsgID 25 sample (same satFeedEventForFrame filter) when the slave DHW bit is clear; SATcontrol.ino satSuppressionFlowTemp() = mean since satCycleGetFlameOnStartMs() when there is a fresh flow and at least one sample, else the latest flow; satPwmSuppressionSetpoint() uses it (tick :1809, 3 s refresh, flood hand-over). Start-up flood (satPwmFlameOffHold / satNoReturnFlood*, entries 45/151) and the continuous follow/clamp (satContinuousSetpoint, entries 336-349) untouched: they call satGetFlowTemp() directly (AC3). Telemetry of the average was planned and is NOT added (left for a follow-up if wanted).

Tests: new tests/test_sat_flow_avg.py compiles SATflowAvg.h + the real satSuppressionFlowTemp() + satPwmSuppressionSetpoint(): 7 passed on the fix; on HEAD (OTGW_SAT_INO = git show HEAD:SATcontrol.ino) 2 fail: a 40->50 step inside the window gives 44.0 on the fix and 49.0 (latest) on HEAD, and the 2 s storage spacing; the window drop-out, pre-ignition exclusion, no-fresh-flow and first-sample cases agree on both (they pin the average's rules). Shared harness change: test_sat_pwm_override.compile_cpp() shims state.otBus.flowAvg to an empty ring of the real header (empty ring = latest flow, so harnesses not about the average keep their behaviour); test_sat_ceiling_paths stubs satSuppressionFlowTemp() with the swept flow; test/host test_boiler_unsupported_origin.cpp and test_local_frame_consumers.cpp include SATflowAvg.h and get SlaveStatus. Sweep: run_tests --changed: 82 SAT files (lowload_contract left out, its delegated files ran directly) and 73 other files green after those fixes; evaluate --quick 88 passed 0 failed.

AC4 evidence (SIMULATION, NOT hardware-verified; no bench per Robert): real MsgID 25 samples of indigo_light's 2026-11-15 heating run 12:52:03-13:09:18 (149 boiler Read-Acks, continuous mode, PIC board) replayed through the real code, CS sampled every 30 s, old (latest) vs new (2 min average): t=90 s 37.70 vs 36.59, 180 s 40.10 vs 38.55, 269 s 40.90 vs 40.39, then within 0.3 K (e.g. 630 s 41.40 vs 41.01, 990 s 40.90 vs 41.10). Open loop: the boiler does not react to the different CS, and that run was continuous mode, not PWM ON; it shows the signal, not the closed-loop effect. Script and output: session 95 scratchpad replay1990.py / replay1990.out. Q-SUP1b (which readings) asked by 92 (78c77ed07); work goes ahead on A.
<!-- SECTION:NOTES:END -->
