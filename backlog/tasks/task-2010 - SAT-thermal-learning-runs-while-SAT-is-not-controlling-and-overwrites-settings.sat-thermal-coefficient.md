---
id: TASK-2010
title: >-
  SAT thermal learning runs while SAT is not controlling and overwrites
  settings.sat thermal coefficient
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T21:08:00Z'
updated_date: '2026-11-17T08:58:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1027000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on .63 (2026-11-16): SATthermalcoeff 0.0665 -> 0.0595 in settings.ini with no boiler (ot_link_lost) and the simulation off. satUpdateThermalLearning() (SATcontrol.ino, the only writer, updateSetting SATthermalcoeff) learns when the flame is off and room and outdoor are valid; it is called before the safety-trip return, satBootSilenceStep() and satOtLinkStep(), so without a boiler a falling room counts as heat loss. Pre-existing. Related: TASK-21 (origin of the feature), TASK-1869 (simulation writes the coefficient). Bug fix on 'not controlling = do not learn', not a SAT behaviour choice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Thermal learning runs only while SAT is actively controlling with a live OT link: no learning during OT link loss, boot silence, safety trip or while SAT is disabled; the source of the feature (thermo-nova or our own) is cited
- [x] #2 Host test on the real satUpdateThermalLearning call path: the coefficient changes with ot_link_lost on origin (red) and stays on the fix (green)
- [x] #3 esp32-combo build, evaluate.py --quick, bench check via 09 (coefficient stays put on .63 without a boiler)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: our own feature (TASK-21 'Thermal drop learning for SAT fallback mode', SATcontrol.ino:7257 banner 'Task #21'); not a thermo-nova port and no Sergeant D entry governs it, so the fix is a bug fix on 'not controlling = do not learn', no SAT behaviour choice. Cause: satUpdateThermalLearning() runs at SATcontrol.ino:7880, before the safety-trip return (:7883), satBootSilenceStep() (:7890) and satOtLinkStep() (:7891); it reads satIsFlameOn(), so with no boiler reply the flame reads off and a falling room counts as heat loss. Fix: satControlLoop() keeps last pass's TASK-1525 gate result (_sat_tuneGatesPassed, set at :7897 once every gate passed: disabled, flashing, MQTT fallback, safety, OT link, boot silence, restart) in _sat_lastPassControlled; the learning branch of satUpdateThermalLearning() runs only when it is true, else it ends the decay period (_thermal_learning = false) so a period never spans a gap. The last-known-room tracking and the TASK-1869 sim switch keep running every pass. One-pass lag accepted (the decay sample is SAT_THERMAL_SAMPLE_MS). Known gap: the first 30 s of a link loss (before bOtLinkLost) still pass the gate; the flame state there is the last boiler reply. Test: new host test on the real satUpdateThermalLearning() + satControlLoop gate wiring: with ot_link_lost (gates fail) and a falling room the coefficient and fThermalCoeff stay; red on origin; existing test_sat_sim_thermal_learning / test_sat_thermal_sim_persist get a stub. Build after ba's 359, with B3a if both ready; bench via 09 on .63.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 implemented (src uncommitted until the build bump): _thermal_lastPassControlled (SATcontrol.ino thermal statics) = _sat_tuneGatesPassed after satControlLoopRun() in satControlLoop(); satUpdateThermalLearning() ends the decay period and returns when it is false (after the last-known-room tracking and the TASK-1869 sim switch), with a debug line 'SAT thermal: learning paused, SAT is not controlling the boiler' on the edge. Origin check: thermo-nova has no thermal drop learning (grep thermal/coefficient in custom_components/sat: only thermal_comfort and heating_curve_coefficient), no Sergeant D entry mentions it, TASK-21 AC1 'during heating-off periods'. Evidence: new tests/test_sat_thermal_learning_gate.py through the real loop harness (whole SATcontrol/SATcycles/SATpid): link down with the thermostat's room frame still coming, room 25 falling 0.1 C/5 min for 2 h, outdoor 8: OLD (OTGW_SATCONTROL_INO = HEAD copy) FAIL saved 0.0500 -> 0.0736; FIX OK, and the link-up positive control learns on both. test_sat_sim_thermal_learning 4 + test_sat_thermal_sim_persist 2 (statics now set controlled=true) + gate 2 + test_sat_loop_integration 21 = 29 passed. Not done yet: full SAT sweep, evaluate, build, bench on .63. Note for Robert: units that ran SAT without a boiler reply may carry a skewed SATthermalcoeff / bThermalModelValid; the fix stops further drift but repairs nothing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pushed in 5fedc50e4..d367abf92 (fix commit 088f52571 rebased, 2.0.0-beta.362; zip post 1572173780229427224). AC1: satUpdateThermalLearning() learns only when the previous satControlLoop() pass got past every gate (_thermal_lastPassControlled = _sat_tuneGatesPassed: disabled, flashing, MQTT fallback, safety trip, boot silence, OT link, restart); otherwise the decay period ends. Source: our own feature (TASK-21), thermo-nova has no thermal drop learning, no Sergeant D entry governs it. AC2: tests/test_sat_thermal_learning_gate.py through the real loop harness (SATcontrol/SATcycles/SATpid whole): link down + room frames + falling room: OLD saved 0.0500 -> 0.0736 FAIL, FIX unchanged OK; link-up positive control learns on both; SAT sweep green except origin-reds owned by others (kd_warmup research sim, pwm_first_start a/a2 from 673f93d7e); lowload contract 35 OK/1 skip; evaluate 89/0. AC3: esp32-combo builds 01d24c5 / 4602739 / d367abf exit 0; bench 09 on .63 (2.0.0-beta.361+01d24c5, bug state SAT enabled, active false, ot_link_lost true, room 24.6 BLE, outdoor 3.2): SATthermalcoeff 0.0420 at 07:43, 08:15 and 09:01 (78 min, 75 samples), settings.ini byte-identical; old code on the same unit drifted 0.0665 -> 0.0595 -> 0.0535 -> 0.0420 overnight; combined image 4602739 health OK on .61 and .63 (heap verdict: boot noise). Not shown on the bench: the 'learning paused' telnet line (it fires only on the edge of a running decay period; .63 booted straight into link loss). Already-drifted coefficients are not repaired: coordinator decision, a release-notes line advises a reset after long boiler-less runs.
<!-- SECTION:FINAL_SUMMARY:END -->
