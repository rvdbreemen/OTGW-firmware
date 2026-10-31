---
id: TASK-1527
title: >-
  SAT heat pumps at low load: minimum switching with maximum comfort (study,
  then design with Sergeant D)
status: To Do
assignee: []
created_date: '2026-10-31T13:48:00Z'
updated_date: '2026-10-31T15:00:00Z'
labels:
  - sat
  - design
dependencies: []
ordinal: 683000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-31: 'de warmtepomp moet minimaal schakelen. Dus goal voor warmtepomp is minimaal aantal schakelingen en maximaal comfort.' Objective for heat pumps: minimise compressor starts, maximise comfort (time within +-0.3 C of the room target). Finding that triggered it (TASK-1519 rounds 4-11, docs/sat/sim-1519/evaluation-2026-10-31.md section 4): in the model a heat pump that modulates only down to 60 % makes about 27-33 starts per day at low load (30 % type: 5-7), indicative only because the model's cycling is not validated against a heat-pump capture. Current firmware: HP PWM period 1800 s (2400 s option, SATcontrol.ino:440-446), MM kept at max in PWM for heat pumps (SATcontrol.ino:453-462, thermo-nova uses 0), max flow 40 C when nothing is chosen (entry 155), min on 180 s. The curve self-tune (ADR-205) cannot reduce starts. Study first, then propose options to Sergeant D one question at a time.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A heat-pump capture (>= 24 h, TASK-1523 tool: flame/compressor, modulation, flow, return, SAT log) is used to validate the model's start count within 25 %
- [ ] #2 Simulation of candidate settings (PWM period, minimum on/off time, flow cap, MM in PWM, hysteresis) scored on compressor starts per day first and comfort second, paired against current settings, over at least 5 winters; data and scripts in docs/sat/sim-hp-lowload/
- [ ] #3 Options with numbers posted to Sergeant D one question at a time; his answers recorded in docs/sat/sergeant-d-corrections.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 round 12 (docs/sat/sim-hp-lowload/round12.md, scripts/sat_hp_onoff_study12.py): on/off ground-source HP on underfloor (maarten_41762 msg 1566089657492705431). own control 2.4 starts/day 0.144 C; time-proportional PWM 180/240 min 7.5/5.6 starts; today's 1800 s PWM 42 starts 0.285 C. Robert (entry 181): two behaviours per setting, 'heat pump controls itself' and 'long time-proportional PWM in minutes per cycle'; Q-1527a to Sergeant D (OK + default).

2026-10-31 Robert asked whether this heat pump can run in continuous mode. Code: the continuous->PWM switch (SATcontrol.ino:6136-6146, satCycleCheckOvershootSwitch SATcycles.ino:1103) applies to every source incl. heat pumps per Sergeant D entry 87 (Q45 A); bOvershootProtection only changes the PWM CS (SATcontrol.ino:1345, 1358), not the entry. So today a HP cannot stay continuous; 'own control' = skip that switch for HPs = changes entry 87 (added to Q-1527a, msg after 1566093012524605644). Model (own, on/off HP, underfloor): on 33-53 % of the time on average, max day 57-84 %, 0 of 106 days fully on (HP sized for -10 C +10 %), median runs 132-406 min, 1.3-4.3 starts/day.

2026-10-31 Robert (entry 182): for heat pumps the firmware may deviate from thermo-nova; parity is no argument for heat pumps. Sergeant D's entries 40/50/87 stay until he revises them (Q-1527a, on the merits).

2026-10-31 round 13 (docs/sat/sim-hp-lowload/round13.md): 16 options, heat-pump mode only, 576 winters. Degree minutes (NIBE method) and room hysteresis dominate long PWM and minimum-run PWM; dm 120: 4.2 starts/day 0.136 C; rh 0.1: 5.3 / 0.102 C; today's pwm 40: 32 / 0.296 C. Recommendation: degree-minute mode for on/off heat pumps (start -120 DM, stop 0, min 20 on / 10 off).

2026-10-31 round 14 (entry 184, docs/sat/sim-hp-lowload/round14.md): third HP PWM option 7200 s. Real-code test via the tests/test_sat_hp_pwm.py harness on patched copies: 9/9 new 7200 s cases pass, 26/27 existing pass (the setter test now sees 3 options, intended). Sim: on/off UFH 42 -> 10 starts/day, abs err 0.29 -> 0.31-0.36; hp30 radiators 6.4 -> 3.4 starts, RMS 0.155 -> 0.331; hp60 30 -> 19, RMS 0.183 -> 0.452. Time-proportional 120 keeps the starts with better comfort; degree minutes best.
<!-- SECTION:NOTES:END -->
