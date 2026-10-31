---
id: TASK-1535
title: >-
  SAT simulation: model a normal 24 kW combi boiler (minimum 20 %, own
  switch-off hysteresis CS+5)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-31T21:50:00Z'
updated_date: '2026-10-31T22:29:00Z'
labels:
  - sat
  - simulation
  - beta.1
dependencies: []
priority: high
ordinal: 691000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-31 (session, interactive choice '24 kW, min 20 %, off at CS+5'): validate with a normal boiler and house instead of oversized-boiler tricks. After TASK-1532 the sim boiler still used a 5 % minimum (1.2 kW, satSimMinMod) and switched off at SAT's own overshoot margin (CS + fOvershootMargin), so the flow never stayed >= CS+2 for 60 s and SAT never reached PWM in simulation. Change: satSimMinMod() 20 % (Geminox quirk floor 10 is below it), flame off at CS + SAT_SIM_BOILER_HYST_K 5 (boiler's own hysteresis, decoupled from SAT's margin). Model check (scratchpad simflame4.py, recompute of the equations): continuous at 2 C outside, low-load cycling and flow >= CS+2 for >20 min from ~8 C, as in field reports (TASK-1412). Values are estimates for a typical combi, not verified per brand.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real satUpdateSimulation(): at a low-load point the flow stays >= CS+2 for at least 60 s after switch-off (red on the 5 %/CS+2 model), and a cold-weather point still burns continuously
- [x] #2 Existing sim tests (test_sat_sim_boiler_model) updated and green; run_tests.py all green
- [x] #3 Bench .61 SAT simulation with a normal setup (24 kW, coefficient 2.0, no oversized boiler): SAT enters PWM by itself, and with source Hybrid the logged PWM period is the heat-pump cycle (closes TASK-1531 AC4)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT simulation models a normal 24 kW combi: satSimMinMod() 20 % (SAT_SIM_MIN_MOD_PCT), boiler switch-off at setpoint + 5 K (SAT_SIM_BOILER_HYST_K) instead of SAT's overshoot margin. Values are estimates for a typical combi (Robert's choice 2026-10-31), not per brand.
Evidence: AC1 tests/test_sat_sim_boiler_model.py (real satUpdateSimulation()+satSimMinMod(), MSVC): new 8/8 OK; old code fails test_low_load_flow_stays_above_cs_plus_2 (flow >= CS+2 for 0 s; new: >= 60 s) and cold weather burns continuously above 20 %. AC2 run_tests.py 80/80, evaluate 0 FAIL. AC3 bench .61 alpha.583+e6bea48, normal setup (24 kW, radiators, coefficient 2.0, sim outdoor ~8.7 C, no oversized boiler): SAT entered PWM by itself within 6 min; bench_check 11/11 PASS (C:/Users/rvdbr/AppData/Local/Temp/claude/D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware/7e8a9b2c-8154-4fbc-badd-02c669e54188/scratchpad/bench-check-192.168.88.61-20261031-232115.txt).
Not verified: the estimates against a real boiler's minimum and hysteresis.
<!-- SECTION:FINAL_SUMMARY:END -->
