---
id: TASK-1419
title: >-
  SAT PWM: heat pumps skip the hourly start limit and enter PWM through the 60 s
  overshoot switch (entries 81, 87; Q36, Q45)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T06:46:00Z'
updated_date: '2026-10-28T22:18:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1382
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 583000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-28: Q36 A (entry 81, no hourly start cap for heat pumps, narrows entry 59) and Q45 A (entry 87, remove the !satAlwaysMaxModulation() gate so heat pumps enter PWM via the 60 s switch, as thermo-nova). Keep recording ON starts so cycles_this_hour is still reported. Q45 only after TASK-1382 (Q35).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: heat pump with 3 PWM starts in an hour is not held (red on HEAD, green on fix); gas boiler regression keeps the entry 59 limit
- [x] #2 Host test: heat pump in sustained overshoot enters PWM after 60 s (red on HEAD, green on fix)
- [x] #3 build + evaluate green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entries relied on: 81 (Q36 A: heat pumps no hourly PWM start cap, as thermo-nova pwm.py:156 never blocks at defaults; gas boilers keep entry 59), 87 (Q45 A: heat pumps enter PWM through the 60 s sustained-overshoot switch, heating_control.py:262-305 has no heat-pump case; exits :307-329 apply too), 40/50 (HP PWM timing: 180 s min ON, period 1800/2400 s, unchanged), 90/95/105/106 (every ON start still recorded; DHW hold unchanged). No conflict found.
Changes:
1. SATcycles.ino satCycleIsHourLimitReached(): keep refreshing iCyclesThisHour, then return false when satGetEffectiveHeatingSource()==SAT_SRC_HEAT_PUMP. satCycleRecordPwmStart() unchanged, so cycles_this_hour is still reported.
2. SATcontrol.ino satGetMaxCyclesPerHour(): drop the HP 'return 2' branch (its only HP reader was the limit; satGetPwmPeriodSec already returns iHpCycleSeconds for HP before using it). Comment updated.
3. SATcontrol.ino auto-switch gate (:5550): 'if (pidOutput > satGetColdSetpoint())' (drop !satAlwaysMaxModulation()); comments at :5544-5553 and SATcycles.ino:160-166 updated (Q45 settled, entry 87).
Scope: SAT_SRC_HYBRID unchanged (already passes the switch gate; keeps the configured cph limit; entry 81 names heat pumps only).
Verification: AC1 new host test in tests/test_sat_pwm_start_limit.py (or hp_pwm): HP source, 3+ ON starts in one rolling hour, the next start is not held; red with OTGW_SAT_INO/OTGW_SATCYCLES_INO pointing at HEAD copies, green on fix; gas regression (4th start blocked) stays green. AC2 host test in tests/test_sat_overshoot_sustain.py: HP source, pidOutput above cold setpoint, sustained overshoot 60 s -> PWM enabled; red on HEAD, green on fix. All SAT suites rerun. AC3 build.bat SUCCESS + evaluate --quick. Docs: firmware-sat-spec.md, sat-comparison.md. Not on otgw-1.x.x (SAT is 2.0.0 only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Self-review 2026-10-28: (a) Q36 premise moved (entry 81): heads-up sent 2026-10-28 06:50Z msg 1564894018377093120 ('Q36 A now simply means heat pumps have no hourly limit, gas boilers keep it'); Sergeant D answered many later questions without objecting; entry 81 Open: none blocking. No conflict open. (b) PWM->continuous exits: grep SATcycles.ino shows no heat-pump / satAlwaysMaxModulation guard, so entry 87 exits already apply. (c) AC2 harness: test_sat_overshoot_sustain.py only extracts SATcycles.ino; the HP gate is in the control loop (SATcontrol.ino:5550). The new test extracts the real if/else gate block from the loop source (HEAD and fix alike) and runs it with the real satCycleCheckAutoSwitch, so red-then-green exercises the changed line. (d) Docs to update in the same commit: firmware-sat-spec.md:1740, sat-comparison.md:481 (they state the HP cap of 2). Change 2 (drop the unreachable HP 'return 2') kept, docs updated alongside.

Implementation 2026-10-28 (uncommitted): SATcycles.ino satCycleIsHourLimitReached returns false for SAT_SRC_HEAT_PUMP after refreshing iCyclesThisHour; SATcontrol.ino satGetMaxCyclesPerHour HP branch removed; auto-switch gate 'if (pidOutput > satGetColdSetpoint())'; satAlwaysMaxModulation() deleted (only caller was the gate); test_sat_hp_pwm.py extract end anchor moved to '// MM= value for the current mode' (works on HEAD and fix). No existing test expected an HP cap of 2 (start_limit stubs the getters), so none converted.
Red/green (rerun by main session): HEAD copies (C:\Users\rvdbr\AppData\Local\Temp/otgw-head-1419): test_sat_pwm_start_limit FAIL test_hp_has_no_hourly_start_limit (Ran 19, failures=1); test_sat_overshoot_sustain FAIL test_heat_pump_overshoot_switches_to_pwm (Ran 21, failures=1). Fix: start_limit Ran 19 OK, overshoot_sustain Ran 21 OK, hp_pwm Ran 22 OK (also OK on HEAD = anchor works). Agent also: anchors 32 OK, override 72 OK, ot_loss 21 OK.

Gates on the uncommitted fix: evaluate --quick Failed 0 exit 0; build.bat esp32-combo SUCCESS (fw 00:05:27, fs 00:00:18, zip 15:58), 0 'unused' warnings.
Review (opus correctness; sonnet tests/rules/SAT entries): no code defect, no conflict with entries 30/40/41/50/52/59/81/82/87/88/90/105/106/107; tests call the real code, red on HEAD for the right reason, regex extraction fail-loud, hybrid covered.
Dispositions: A (cycles_this_hour ring saturates at 6, now reachable for HP) -> TASK-1425. B (HP MM in PWM = user max vs 100 outside PWM; entry 21 Open) -> TASK-1426 (question for Sergeant D). C/B7 (entry 87 names TASK-1412.3 as a dependency): waived; 1412.3 is the epic-wide contract-regression suite, and this task carries its own executable regressions (start_limit hplimit, overshoot GateHeatPumpSwitch) that 1412.3 will absorb; no code in 1412.3 is a prerequisite. D/A1 (start_limit stubs period/cph for every source): accepted, the test proves the exemption branch; HP timing is covered by test_sat_hp_pwm.py. A2 (underfloor cold setpoint not in gate test) and A3 (HP exits untested): accepted low; exits have no source check, so the existing gas exit tests exercise the same code. E/B1-B6 stale docs and history comments: fixed in this task (docs agent).

2026-10-28 bench old vs new on the Classic+S3 (.63), replayed boiler, old = alpha.519+49135fe (same SAT code as alpha.520 for gas), new = alpha.521+2425108, overshoot margin 3.0 (bench value, not the decided default 2.0). Logs: %TEMP%\otgw-bench-1420\{A_519,C_521}. Raw lines checked by the main session. S5 heat pump overshoot: old (alpha.519) stays continuous for 240 s of flow 41-42, no switch line; new switches at 60 s '(flow=41.4 >= sp=33.6 + 3.0)' 22:49:44 with PWM ON and CS on the same tick; underheat exit fires for the heat pump at 180 s. S6 hourly limit (cph 2, manual mode toggles): gas both builds blocked at the 3rd start ('cycle limit 2/hr reached'); after satsource=2 old stays at cycles_this_hour 2 / pwm_flame_req false, new goes 3 then 4 with pwm_flame_req true. Not shown: a real heat pump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heat pumps get no hourly PWM start limit (entry 81, Q36 A; gas and hybrid keep entry 59) and enter PWM through the 60 s sustained-overshoot switch like every source (entry 87, Q45 A; thermo-nova heating_control.py:262-305 has no heat-pump case, pwm.py:156 never blocks at defaults). satCycleIsHourLimitReached returns false for SAT_SRC_HEAT_PUMP after refreshing cycles_this_hour (starts still recorded and reported); satGetMaxCyclesPerHour heat-pump branch and satAlwaysMaxModulation() removed; the PWM->continuous exits now also run for heat pumps (no source check in SATcycles.ino).
AC1: tests/test_sat_pwm_start_limit.py test_hp_has_no_hourly_start_limit FAIL on HEAD copies ('1' != '0', Ran 19 failures=1), OK on fix (Ran 19 OK); test_gas_and_hybrid_keep_the_hourly_start_limit and the existing 4th-start-blocked test green on both.
AC2: tests/test_sat_overshoot_sustain.py GateHeatPumpSwitch (real if/else gate cut from SATcontrol.ino, real satCycleCheckAutoSwitch and getters) test_heat_pump_overshoot_switches_to_pwm FAIL on HEAD (('-1','1') != ('61000','2')), OK on fix (Ran 21 OK); gas/hybrid switch on both.
AC3: build.bat esp32-combo SUCCESS (fw 5:27, fs 0:18); evaluate --quick Failed 0. Other suites: anchors 32, hp_pwm 22, override 72, ot_loss 21 OK.
Docs: firmware-sat-spec.md, sat-comparison.md, c4-component-smart-thermostat.md updated. Review: no code defect, no entry conflict; follow-ups TASK-1425 (cycles_this_hour ring caps at 6) and TASK-1426 (heat-pump MM in PWM, entry 21 open). TASK-1412.3 dependency waived (reason in notes).
Not verified: a real heat pump; no bench run (no heat-pump boiler on the bench).
<!-- SECTION:FINAL_SUMMARY:END -->
