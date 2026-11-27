---
id: TASK-3143
title: >-
  SAT: send MM=T only in continuous mode; in PWM keep MM=0 (Sergeant D entry
  505, scope 2.0.0 or 2.1.0 not decided)
status: To Do
assignee: []
created_date: '2026-11-27T21:27:00Z'
updated_date: '2026-11-27T21:39:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, 2026-11-27 (entry 505): MM=T only when SAT is in continuous mode; in PWM MM stays 0. Today the firmware sends MM=T inside the PWM ON phase at the maximum duty range (TASK-1581, entries 242, 243, 246; SATcontrol.ino satPwmAtMaxDuty, satSendMM). Interpretation (ours): PW' = PWM. Release scope not decided, to be asked of Sergeant D. SAT control behaviour: the plan quotes thermo-nova and waits for his OK before code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The plan quotes thermo-nova heating_control.py (file:line at 7beaaf5) and every earlier MM entry (21, 44, 113, 237, 239, 242, 243, 246, 269) and states which are superseded by entry 505
- [ ] #2 Release scope (2.0.0 or 2.1.0) decided by Sergeant D
- [ ] #3 In PWM mode (ON and OFF phase) SAT never sends MM=T: host test red on the old code, green on the fix
- [ ] #4 In continuous mode MM behaviour is unchanged except as entry 505 says; the hand-back MM=T at SAT disable (entry 47) is unchanged
- [ ] #5 Bench with the SAT simulator shows no MM=T during a PWM ON phase and the sattick log mm column matches
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Entry 505 (register OTGW-sat-decisions). Thermo-nova citations heating_control.py:510-523 and :133-139 at 7beaaf5 are cited, not yet confirmed by a second reader (otgw-architect to open both locations and try to refute; his verdict goes here). Release scope 2.0.0 is provisional until Sergeant D answers the scope question. Firmware send site: SATcontrol.ino:8914 (mmTarget) and :8921 (satSendMM); SAT_MM_T in satSendMM :758-771.

Second reader (otgw-architect): thermo-nova never sends MM=T (confirmed); MM=0 in PWM has exceptions: hot water (maximum, default 100, heating_control.py:136-137, :519-520), Geminox (10, :522-523), no modulation management (nothing set, :512-515); state OFF follows _pwm.enabled (:139-140), so MM=0 also with PWM enabled and IDLE. The plan must treat hot water (entry 220: MM max during a draw) as unchanged and ask Sergeant D with the scope question. Notes: OTGW-field-data/_comms/second_reader_entry505_mm_pwm_2026-11-27.md.

Entry 507 (Sergeant D, 2026-11-27 22:3x UTC, 'my answer is A'): release scope 2.0.0, built with host tests and the SAT simulation bench first. Our fallback (move to 2.1.0 if the simulation bench does not pass in time) is ours, not his; put to him again before it is taken. Milestone 2.0.0 is final.

PLAN (otgw-poc-dev, 2026-11-27, entries 505 and 507; no code before the telemetry candidate is on origin/dev, one build slot per change). A. Change in SATcontrol.ino (line numbers of origin/dev 78646cc99): (1) :8914 'const uint8_t mmTarget = (!coldCutoff && satPwmAtMaxDuty()) ? SAT_MM_T : state.sat.iCurrentModulation;' becomes 'mmTarget = state.sat.iCurrentModulation' (satComputeMM, :536-544, already gives 0 in PWM for a gas boiler, the configured maximum for a heat pump or hybrid, floor 10 for Geminox); satSendMM(mmTarget) at :8921 is unchanged. (2) :8863-8866 the satReportedModulation(satPwmAtMaxDuty()...) override of current_modulation goes: with no MM=T in PWM there is nothing to report (entry 505: whether entry 269 still applies in continuous mode is open: SAT sends the configured maximum there, never MM=T, so current_modulation is simply that value). (3) Dead code, in a second commit: satPwmAtMaxDuty() (:1240) and _pwm_mmRelease (:1236, TASK-2992 entries 436/440) and the SAT_MM_T marker with its TP=11:12 special case (:750-771, entry 246) go; KEEP _pwm_atMaxDuty and satPwmInMaxDutyRange() (:1235, :1242): the heating-failure 'at maximum' input (:8904) reads them. The hand-back MM=T of satDisable() and the restart hand-back (:885, entry 47) is a direct addCommandToQueue and stays.

B. Unchanged by this task, on purpose (entry 505, entry 507 'Open'): heat pump, hybrid and Geminox in PWM keep today's rule (satComputeMM: configured maximum, never 0, floor 10: entries 21, 113); the hot-water draw keeps MM at the maximum (entry 220: SATcontrol.ino:8573, :8603, :8619); Immergas keeps TP=11:12=min(mm,80) before every MM= (entry 233), now carrying 0 in PWM; the sending rules (on change, 60 s repeat: entries 44, 237, 239) are unchanged. The three open points go to Sergeant D only via the coordinator: the hot-water question (entry 220 A yes / B no, advice A), heat pump/hybrid/Geminox in PWM, entry 269. thermo-nova alignment (second reader CONFIRMED): heating_control.py:134-146 (state OFF while self._pwm.enabled, hot water first), :510-523 (value 0 in OFF, maximum otherwise, Geminox >= 10, no value without relative modulation management), :237 (sent every update); thermo-nova never sends MM=T. One difference to keep in view: thermo-nova keys MM=0 on _pwm.enabled, the firmware on eControlMode == SAT_MODE_PWM (satComputeMM(pwmMode) at :8858): the two differ only in the window where PWM is enabled but the mode is already continuous or not yet PWM; the host test pins the firmware's rule and lists that window as a characterization case, not as a change.

C. Host tests (red on the old code, green on the fix): new tests/test_sat_mm_pwm_zero_3143.py on the loop harness (tests/sat_loop_build.py, which compiles the real control tick): (1) PWM ON phase at the maximum duty range (raw duty above 1.03, the old _pwm_mmRelease): gas boiler sends MM=0 and never MM=T (red now: MM=T); (2) PWM ON phase below the maximum duty and the OFF phase: MM=0 (green now, characterization); (3) continuous mode: configured maximum (100 by default), never T; (4) heat pump and hybrid in PWM at the maximum duty: configured maximum, never 0, never T (entries 21, 113); (5) Geminox in PWM: 10; (6) hot-water draw inside a PWM ON phase: maximum (entry 220); (7) Immergas: TP=11:12=0 then MM=0 in PWM at the maximum duty, no TP=80 / MM=T pair; (8) SAT disable and restart hand-back still send MM=T (entry 47, tests/test_sat_disable_handback.py unchanged and green); (9) cold cutoff: unchanged (MM max restore). Existing tests that assert MM=T at the maximum duty change to MM=0 in the same commit (found by grep on origin/dev: tests/test_sat_pwm_anchors.py, test_sat_pwm_live_flow_duty_2993.py, test_sat_cmd_cache.py, test_sat_telemetry_wire.py, test_sat_requested_vs_transmitted.py, test_sat_loop_integration.py, test_sat_immergas_tp.py, test_sat_command_delivery.py, test_sat_lowload_contract.py, test_sat_cold_cut_min_burn.py: each file is read before it is edited; the hand-back assertions stay). D. Bench (AC5): SAT simulator on a unit, sattick mm column shows no T during a PWM ON phase, matches MM=0 in the PIC command log; the full suite (python scripts/run_tests.py) before the build slot. E. Docs: docs/sat/firmware-sat-spec.md (the MM paragraph), docs/api/MQTT.md (current_modulation), release-notes line: 'SAT no longer sends MM=T during PWM (Sergeant D entry 505)'. Evidence still missing: nothing here was run; the line numbers are from origin/dev 78646cc99.
<!-- SECTION:NOTES:END -->
