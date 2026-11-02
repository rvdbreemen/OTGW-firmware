---
id: TASK-1527
title: >-
  SAT heat pumps at low load: minimum switching with maximum comfort (study,
  then design with Sergeant D)
status: In Progress
assignee: []
created_date: '2026-10-31T13:48:00Z'
updated_date: '2026-11-02T22:13:00Z'
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
- [x] #2 Simulation of candidate settings (PWM period, minimum on/off time, flow cap, MM in PWM, hysteresis) scored on compressor starts per day first and comfort second, paired against current settings, over at least 5 winters; data and scripts in docs/sat/sim-hp-lowload/
- [x] #3 Options with numbers posted to Sergeant D one question at a time; his answers recorded in docs/sat/sergeant-d-corrections.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31 round 12 (docs/sat/sim-hp-lowload/round12.md, scripts/sat_hp_onoff_study12.py): on/off ground-source HP on underfloor (maarten_41762 msg 1566089657492705431). own control 2.4 starts/day 0.144 C; time-proportional PWM 180/240 min 7.5/5.6 starts; today's 1800 s PWM 42 starts 0.285 C. Robert (entry 181): two behaviours per setting, 'heat pump controls itself' and 'long time-proportional PWM in minutes per cycle'; Q-1527a to Sergeant D (OK + default).

2026-10-31 Robert asked whether this heat pump can run in continuous mode. Code: the continuous->PWM switch (SATcontrol.ino:6136-6146, satCycleCheckOvershootSwitch SATcycles.ino:1103) applies to every source incl. heat pumps per Sergeant D entry 87 (Q45 A); bOvershootProtection only changes the PWM CS (SATcontrol.ino:1345, 1358), not the entry. So today a HP cannot stay continuous; 'own control' = skip that switch for HPs = changes entry 87 (added to Q-1527a, msg after 1566093012524605644). Model (own, on/off HP, underfloor): on 33-53 % of the time on average, max day 57-84 %, 0 of 106 days fully on (HP sized for -10 C +10 %), median runs 132-406 min, 1.3-4.3 starts/day.

2026-10-31 Robert (entry 182): for heat pumps the firmware may deviate from thermo-nova; parity is no argument for heat pumps. Sergeant D's entries 40/50/87 stay until he revises them (Q-1527a, on the merits).

2026-10-31 round 13 (docs/sat/sim-hp-lowload/round13.md): 16 options, heat-pump mode only, 576 winters. Degree minutes (NIBE method) and room hysteresis dominate long PWM and minimum-run PWM; dm 120: 4.2 starts/day 0.136 C; rh 0.1: 5.3 / 0.102 C; today's pwm 40: 32 / 0.296 C. Recommendation: degree-minute mode for on/off heat pumps (start -120 DM, stop 0, min 20 on / 10 off).

2026-10-31 round 14 (entry 184, docs/sat/sim-hp-lowload/round14.md): third HP PWM option 7200 s. Real-code test via the tests/test_sat_hp_pwm.py harness on patched copies: 9/9 new 7200 s cases pass, 26/27 existing pass (the setter test now sees 3 options, intended). Sim: on/off UFH 42 -> 10 starts/day, abs err 0.29 -> 0.31-0.36; hp30 radiators 6.4 -> 3.4 starts, RMS 0.155 -> 0.331; hp60 30 -> 19, RMS 0.183 -> 0.452. Time-proportional 120 keeps the starts with better comfort; degree minutes best.

2026-10-31 rounds 16-17 (docs/sat/sim-hp-lowload/round16.md, round17.md, charts round16_vs_current.png, round17_period_sweep.png): degree minutes on all HP types vs today's SAT PWM 30 min, and SAT's own PWM extended to 240 min. Extending SAT PWM always trades starts for comfort (fixed ON time per period); at equal starts degree minutes are far better on radiators (hp30 3.7 starts 0.224 C vs PWM120 3.6 starts 0.365 C); on underfloor time-proportional improves with period, SAT PWM worsens; DM best. DM benefit depends on emitter mass (UFH -90 % starts and better comfort; radiators trade-off).

2026-10-31 entry 186 (Hybrid = heat pump, scope open as Q-1531a): read against rounds 15-17. Low-load PWM is the regime where a hybrid runs its heat pump alone (the gas boiler assists only at high load), so the heat-pump results (HP PWM cycle, degree minutes) apply to a hybrid at low load without a new simulation. Not modelled: switch-over between heat pump and gas boiler, and gas assist during a degree-minute run. A hybrid case is only needed if Q-1531a = B (every heat-pump rule, incl. max flow 40) or if a hybrid tester reports switch-over behaviour.

2026-10-31 Q-1531a answered A (Sergeant D 19:12 UTC, msg 1566167931191304233; ea records it as entry 188): a hybrid uses the heat-pump PWM cycle, the rest stays gas. So no hybrid simulation is needed. Consequence for the TASK-1527 design: whatever replaces the heat-pump PWM (degree minutes or a longer cycle, Q-1527a) also reaches the hybrid's heat pump, while the gas start limit and the max-flow defaults stay. Q-1527a must say so explicitly.

2026-11-01: Sergeant D's position on degree minutes (comfort vs efficiency, spark gap, minimum capacity is the lever) recorded as entry 192; answered in #dev-sat-mqtt msgs 1566380852735254659 / 1566381192104779827 with analysis of his UK charts. Field test plan (switchback, same house, both methods, pre-registered H1-H3) in docs/sat/sim-hp-lowload/field-test-plan.md. Next in the model: round 18 = start-loss term (Cd 0.05/0.10), spark-gap cost with SAT-on-gas reference, and the field protocol simulated to size the test. Q-1527a still open.

2026-11-01 Robert: field test without a new control mechanism, only longer heat-pump PWM periods 1.5/2/3 h (entry 193) -> TASK-1542; round 18 deferred -> TASK-1543 (label simulatie); testers may be asked. field-test-plan.md updated to arms 1800 s vs 10800 s (underfloor) / 5400 s (radiators).

Audit 2026-11-01: moved To Do -> In Progress. Simulation rounds 12-17 are committed (9 commits, e.g. 7c718324d, 71a8b78f1, a38973b14, 9a6815991). Still open: AC1 (24 h heat-pump capture validating the start count within 25 %) and Sergeant D's answer to Q-1527a.

2026-11-01 AC#2 check (lead asked): NOT met yet, evidence per item. Done, paired against current settings, starts first then comfort: PWM period 30-240 min (rounds 14, 17, 18b), minimum ON 3/10/60 min and OFF 10 min (18b/18c/18d), room hysteresis (13), degree minutes and own control (13, 15, 16, 19c), curve (19b/19c), setback and room feedback (19). Gaps: (1) winters: rounds 12-19 ran 3 heating seasons x 2 seeds (round 13 = 576 runs over 3 winters, not 576 winters); the >= 5 winters requirement is only met by round 21 (10 seasons 2015-16..2024-25, SAT through the proven port, current vs continuous), which is still running; (2) flow cap and MM in PWM were not simulated as candidate settings. Plan to close AC#2: after round 21, rerun the candidate sweep (period, min ON/OFF, flow cap 35/40/45, MM in PWM 100 vs minimum) over the ten seasons with scripts/sat_hp_library_study21.py.

2026-11-02 AC#2 checked with evidence: docs/sat/sim-hp-lowload/round23.md (round 23, 840 runs; round 23b, 360 runs), ten heating seasons 2015-16..2024-25, 3 units (min 1.8/2.4/3.2 kW) x 4 houses, SAT through the port proven against the firmware (tests/test_sat_sim_port_equivalence.py 4/4 OK on beta.2). Every candidate paired against current (period 1800 s, 600/600 s, cap 40 C, MM 100): period 3600 s -42 % starts at +0.002 C, 10800 s -77 % at +0.008 C; old 180 s rule worse in every run (+1.1 starts, worse comfort in 92 %); flow cap 35/45 C no effect on underfloor (limit: radiators not tested); MM 0 in PWM -0.4 starts; hysteresis (heat pump's own band, SAT continuous) 1/1 K 9.9 starts 0.111 C, 3/1 K 3.4 / 0.136, 5/3 K 1.7 / 0.181. Best overall on both scores: SAT continuous. Scripts in scripts/ (sat_hp_candidate_sweep_study23.py, sat_hp_library_study21.py, sat_sim_port.py), data and reports in docs/sat/sim-hp-lowload/. Rounds 21/22 reports: round21.md, round22.md.

2026-11-02 AC#2 unchecked again (self-review): round 23b swept the heat pump's own band, not a SAT setting. The SAT-side hysteresis is the overshoot switch into PWM (entry 87, fOvershootMargin 2.0, 60 s). Round 23c runs it (margin 3.0 = thermo-nova cycles/const.py:10, margin 4.0, 300 s dwell) plus period 2400 s, the selectable middle value (beta.4 setter snaps to 1800/2400/10800). Found while doing so: rounds 21-23 switch on the first qualifying minute, the firmware one tick later (satCycleCheckOvershootSwitch, 60 s from the first qualifying tick); 23c includes the firmware-faithful variant.

2026-11-02 AC#2 checked again, now with SAT's own hysteresis: round 23c (600 runs, ten seasons, paired against current; docs/sat/sim-hp-lowload/round23.md findings 6-9, round23c_switch_runs.json). Period 2400 s (selectable) -19 % starts (17.0 -> 13.8/day) at +0.0007 C, 120/120 runs fewer starts. Switch as the firmware (one tick later than the model) -0.5 starts/day: rounds 21-23 conclusions stand. Dwell 300 s -0.9. Margin 3.0/4.0: SAT never enters PWM in the model, an artefact of the assumed heat-pump stop band (stops at CS+3 K, study21.py:206); with a 5 K band margin 3.0 gives 12.4 starts/day, 4.0 gives 9.0 (NIBE, 2015 house, 2024-25). Rule: the switch fires only when the unit's own stop overshoot exceeds SAT's margin; needs a capture (AC#1). Data added to Q-1527c (advice default 2400 s), new queued Q-1527f (margin). Port proof 4/4 OK on beta.5 (0ab3edd99).

2026-11-02 AC#3: options with numbers put to Sergeant D (Q-1527c with round 23 data, Q-1527f with round 23c, Q-1527a confirm) in the one-time overview of 04:53 (#alpha-testing msg 1566676722768941168; one overview instead of one question at a time, maintainer exception). Answers 05:31 (msg 1566686069796114443) recorded as entries 227 (Q-1527a A), 228 (Q-1527c C: 60 min as a new choice, default unchanged = our interpretation, TASK-1557), 229 (Q-1527f C: margin after a heat-pump capture). Entry 210 (slave SAT rationale) recorded; round 22b (5.5 kW boiler minimum, return-temperature efficiency, start losses) running. Remaining: AC#1 (heat-pump capture).

2026-11-02 evening: AC#1 still open. Asked maarten_41762 (Remeha/Techneco Toros on/off heat pump) for a >= 24 h long-run capture (msg 1566903649655853077, correction 1566903751040573582). Robert 2026-11-02: field data (24 h) expected soon; if that does not work out, the task may be closed without AC#1 (waiver, named in the Final Summary). maarten 21:23 UTC (msg 1566925806268186755): away until Thursday, no capture before then; written summary: ~9 kW on/off unit, 2017 A-label 175 m2 house, high-mass UFH, heat recovery; NOT SAT-driven (Honeywell floor controller, 180 min PWM, blocked 06-10 and 16-21); 1 and 2 Nov: 3 runs/day of 40-45 min, supply ~29.5 C, rooms 20.5-21 C. Not an AC#1 validation (SAT not in control, no capture); consistent with the model's long-period own-control range (round 12: 2.4 starts/day). Asked for a SAT-driven capture after Thursday (msg 1566938112548937809).
<!-- SECTION:NOTES:END -->
