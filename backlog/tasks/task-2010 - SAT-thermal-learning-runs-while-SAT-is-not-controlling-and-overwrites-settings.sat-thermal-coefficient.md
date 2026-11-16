---
id: TASK-2010
title: >-
  SAT thermal learning runs while SAT is not controlling and overwrites
  settings.sat thermal coefficient
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T21:08:00Z'
updated_date: '2026-11-16T21:40:00Z'
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
- [ ] #1 Thermal learning runs only while SAT is actively controlling with a live OT link: no learning during OT link loss, boot silence, safety trip or while SAT is disabled; the source of the feature (thermo-nova or our own) is cited
- [ ] #2 Host test on the real satUpdateThermalLearning call path: the coefficient changes with ot_link_lost on origin (red) and stays on the fix (green)
- [ ] #3 esp32-combo build, evaluate.py --quick, bench check via 09 (coefficient stays put on .63 without a boiler)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Source: our own feature (TASK-21 'Thermal drop learning for SAT fallback mode', SATcontrol.ino:7257 banner 'Task #21'); not a thermo-nova port and no Sergeant D entry governs it, so the fix is a bug fix on 'not controlling = do not learn', no SAT behaviour choice. Cause: satUpdateThermalLearning() runs at SATcontrol.ino:7880, before the safety-trip return (:7883), satBootSilenceStep() (:7890) and satOtLinkStep() (:7891); it reads satIsFlameOn(), so with no boiler reply the flame reads off and a falling room counts as heat loss. Fix: satControlLoop() keeps last pass's TASK-1525 gate result (_sat_tuneGatesPassed, set at :7897 once every gate passed: disabled, flashing, MQTT fallback, safety, OT link, boot silence, restart) in _sat_lastPassControlled; the learning branch of satUpdateThermalLearning() runs only when it is true, else it ends the decay period (_thermal_learning = false) so a period never spans a gap. The last-known-room tracking and the TASK-1869 sim switch keep running every pass. One-pass lag accepted (the decay sample is SAT_THERMAL_SAMPLE_MS). Known gap: the first 30 s of a link loss (before bOtLinkLost) still pass the gate; the flame state there is the last boiler reply. Test: new host test on the real satUpdateThermalLearning() + satControlLoop gate wiring: with ot_link_lost (gates fail) and a falling room the coefficient and fThermalCoeff stay; red on origin; existing test_sat_sim_thermal_learning / test_sat_thermal_sim_persist get a stub. Build after ba's 359, with B3a if both ready; bench via 09 on .63.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 implemented (src uncommitted until the build bump): _thermal_lastPassControlled (SATcontrol.ino thermal statics) = _sat_tuneGatesPassed after satControlLoopRun() in satControlLoop(); satUpdateThermalLearning() ends the decay period and returns when it is false (after the last-known-room tracking and the TASK-1869 sim switch), with a debug line 'SAT thermal: learning paused, SAT is not controlling the boiler' on the edge. Origin check: thermo-nova has no thermal drop learning (grep thermal/coefficient in custom_components/sat: only thermal_comfort and heating_curve_coefficient), no Sergeant D entry mentions it, TASK-21 AC1 'during heating-off periods'. Evidence: new tests/test_sat_thermal_learning_gate.py through the real loop harness (whole SATcontrol/SATcycles/SATpid): link down with the thermostat's room frame still coming, room 25 falling 0.1 C/5 min for 2 h, outdoor 8: OLD (OTGW_SATCONTROL_INO = HEAD copy) FAIL saved 0.0500 -> 0.0736; FIX OK, and the link-up positive control learns on both. test_sat_sim_thermal_learning 4 + test_sat_thermal_sim_persist 2 (statics now set controlled=true) + gate 2 + test_sat_loop_integration 21 = 29 passed. Not done yet: full SAT sweep, evaluate, build, bench on .63. Note for Robert: units that ran SAT without a boiler reply may carry a skewed SATthermalcoeff / bThermalModelValid; the fix stops further drift but repairs nothing.
<!-- SECTION:NOTES:END -->
