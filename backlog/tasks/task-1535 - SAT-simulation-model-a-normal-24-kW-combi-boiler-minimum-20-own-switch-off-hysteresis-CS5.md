---
id: TASK-1535
title: >-
  SAT simulation: model a normal 24 kW combi boiler (minimum 20 %, own
  switch-off hysteresis CS+5)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T21:50:00Z'
updated_date: '2026-10-31T21:52:00Z'
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
- [ ] #1 Host test on the real satUpdateSimulation(): at a low-load point the flow stays >= CS+2 for at least 60 s after switch-off (red on the 5 %/CS+2 model), and a cold-weather point still burns continuously
- [ ] #2 Existing sim tests (test_sat_sim_boiler_model) updated and green; run_tests.py all green
- [ ] #3 Bench .61 SAT simulation with a normal setup (24 kW, coefficient 2.0, no oversized boiler): SAT enters PWM by itself, and with source Hybrid the logged PWM period is the heat-pump cycle (closes TASK-1531 AC4)
<!-- AC:END -->
