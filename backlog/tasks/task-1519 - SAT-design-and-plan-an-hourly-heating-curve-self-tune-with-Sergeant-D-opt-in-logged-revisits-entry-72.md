---
id: TASK-1519
title: >-
  SAT: design and plan an hourly heating-curve self-tune with Sergeant D
  (opt-in, logged); revisits entry 72
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T05:53:00Z'
updated_date: '2026-10-31T13:21:00Z'
labels:
  - sat
  - design
dependencies: []
priority: high
ordinal: 675000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-31 05:51 UTC in #alpha-testing (msg 1565966404304248853): 'I want the self tune feature of the heating curve back. I want an hourly evaluation. And based on that we can change the heat curve. Can we design and plan that feature with sergeant. ... This should become a feature that can be turned off and on. Also I want you to report in the logs that you are tuning the curve when you do so.' Context: TASK-1413 removed the old firmware-only auto-tune on Sergeant D's entry 72 (Q22 'Remove'), asked as a parity question. This is a new design, not a restore of the old code (it scaled the coefficient by a rate after 1 h + 6 mostly-overshoot cycles, clamped 0.3-5.0, wrote settings.ini each time). Design points to settle with Sergeant D, one question at a time: (1) the evaluation signal (overshoot/underheat fractions of the classified cycles, room error, curve recommendation?), (2) step size and bounds, (3) minimum data before a step and what blocks it (DHW, PWM, missing outside temperature), (4) flash-write cadence (persist on change, rate-limited), (5) how it relates to the existing curve recommendation. Robert's fixed requirements: hourly evaluation, on/off setting (default to be decided), a log line (telnet + SAT log) for every change with old and new coefficient and the reason.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Sergeant D's answers on the design points recorded in docs/sat/sergeant-d-corrections.md (entry 72 is revisited by name)
- [ ] #2 An ADR (adr-kit) records the design: signal, step, bounds, blocks, write cadence, setting and default
- [ ] #3 Implementation tasks filed from the ADR with host-test ACs (red/green) and a bench AC
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-31: Robert asked to start now. Design question Q-1519a posted to Sergeant D 05:55 UTC (msg 1565967378234347582) with proposal A (integral offload) and alternatives B (daily HCR auto-apply) and C (classified cycles). Waiting for his answer; record it in sergeant-d-corrections.md before any design or code.

2026-10-31 simulation (Robert asked 06:02, msg 1565969242921246821): scripts/sat_curve_selftune_sim.py (thermo-nova curve + PID at the pin; invented one-mass house, continuous modulation only). Results posted msg 1565970072823144611: A (integral sign) never moves with a too-low curve (integral zeroed outside DEADBAND 0.1, pid.py _update_integral) and hunts inside it (325-379 writes/3 weeks); A2 (mean error) and B (daily HCR) do not see a too-high curve (kp = c*curve/3 hides it); A3 (hourly mean PID correction = setpoint - curve > 1.5 C flow, 0.1 step, max 1 step / 3 h) converges both ways, RMS 0.04-0.06 C, 2-10 writes. Best coefficient varies with outside temperature in this model (1.4 at -10 C to 2.6 at +10 C). Q-1519a proposal corrected from A to A3 in the same post.

2026-10-31 06:10-06:28 Sergeant D in #dev-sat-mqtt: entries 159 (channel), 160 (sun elevation < 12 deg), 161 (SAT in charge, integral as the clue, no sun). Conflict with the deadband integral put back as Q-1519b (msg 1565976153540530227), advice: a separate tuner 'curve integral' (A3).

2026-10-31 06:55 Robert: run TASK-1519 with high priority. 06:56 Sergeant D entry 166: most homes need 1.6-2.4. Study: house model validated on gruijter's alpha.549-era graph capture (4 h, 2870 samples): ratios K/H ~ 1 and tau ~ 110 h identifiable (RMSE 0.033 C fit half, 0.069 C held-out half), absolute scale not (ridge). Real weather Open-Meteo archive 52.1N 6.2E 2025-11-01..2026-02-28. Study scripts/sat_curve_selftune_study.py: 63 strategies x 18 houses (tau 40/110/150 h, K/H 0.6/1.0/1.6, solar 0.5/2.5 m2) x 5 start offsets x 2 seeds. K/H to best coefficient at 4.8 C: 0.6->3.0, 0.75->2.4, 0.8->2.3, 1.0->1.8, 1.2->1.5, 1.6->1.1; realistic subset per entry 166 = K/H 0.75-1.2.

2026-10-31 07:23 Sergeant D entry 168 (walls = thermal battery, ~3 days, response 60-72 h). 07:30 Robert: research thermal battery, house model by Dutch regulation eras and German codes, show how to use the battery. Research agent running -> docs/sat/sim-1519/house-typology-research.md; next model = two-node (air + wall) RC.

2026-10-31 08:19 Robert: round 5 = firmware + thermo-nova settings for gas and heat pumps, explore the optimum, explain the current choice. 08:27 Robert: detect oscillation; estimate thermal mass from cool-down behaviour. Firmware defaults read: coeff 2.0, fOvershootMargin 2.0 (TASK-1268; thermo-nova 3.0), iCyclesPerHour 3, fMaxSetpoint 65 global, SAT_MAXTSET_HEAT_PUMP_CAP 40 (entry 155), SAT_MIN_SETPOINT 10, fFlameOffOffset 18, fModSupOffset 1.0. Round 3 first run had an unclamped setpoint (model error), rerun with the [10, 80] clamp; round 4 restarted after it.

2026-10-31 simulation plan posted (msg 1566008766565195867): ~33,300 winters. R1 11,520 done; R2 5,940 done; R3 3,960 rerun (clamp); R4 1,200 queued (gas + HP30/HP60, PWM per heating_control.py:262-350); R5 10,656 (firmware settings, overshoot margin 2.0 vs 3.0, 8 houses x 3 sources x 3 starts x 2 seeds x 37 tuners: window 48/72/96 h, threshold 0.5/1/1.5, step 0.05/0.1, band none/+-0.4, + none); R6 36 winters + trace re-analysis (time-constant estimate from night cool-down, oscillation detection).

2026-10-31 round 4 (indicative; boiler cycling not validated: flame fraction matches the field at 2000 W/K boiler gain, starts/h do not). Design rule found: exclude SAT-PWM hours from the tuner's clean hours. Eras 1983+, gas, KISS daily: PWM counted 20.5 writes / 4.0 flips / 271 min/d out of band (no tuner 262); PWM excluded 4.1 writes / 0.2 flips / 200 min/d, RMS 0.261 -> 0.215 (scratchpad curvesim/pwmtest.py, EXCLUDE_PWM flag in scripts/sat_curve_selftune_study4.py). Heat pump 30 %: tuner still slightly worse than none (0.212 vs 0.194 RMS), open. Gas effect of tuning < 1 %; aim-low saves 2-8 % HP electricity at a large comfort cost. Posted msg 1566012204472471723.

2026-10-31 HANDOVER (Robert: alpha session leaves TASK-1519 to the dev session). Done: rounds 1-4 and 6 posted + pushed. Round 5 finished (7,992 runs, firmware settings margin 2.0/3.0, max 62/40, PWM hours excluded, 36 daily variants): raw results docs/sat/sim-1519/data/round5_results_raw.json, script scripts/sat_curve_selftune_study5.py; NOT yet analysed or posted. Open: (1) round 5 analysis + optimum explanation; (2) Sergeant D msg 1566008707366785035 asks a re-run with the curve derivative (derivative not frozen in the deadband); holding answer msg 1566021979608322139 gives our reading and asks him to confirm, run as round 7; (3) heat-pump tuner result unexplained (round 4); (4) Q-1519d/e waiting.

2026-10-31 ~10:00-10:50 UTC (session 9c took over the study from session ea, Robert's instruction). Round 5 (7,992 runs) analysed: its reference best_coeff2 (study3.py:30-33, flow = mean + 5 C, no gains) was 1.80 everywhere and wrong for the round-4 hydraulics (flow = mean + Q/2MCP, ~135 W gains); signal ablation: clamping < 2 % of hours and positive (not the cause), minimum-output hours strongly negative, modulating hours at 1.8 also negative. Round 4/5 claim 'tuner does not help heat pumps' withdrawn (posted msgs 1566028632797880353/1566028634341384213). Round 5b: radiators calibrated per house to balance at 1.8 (entry 166 anchor), fixed sweep shows cliff below ~1.2, plateau 1.3-2.4; 630 paired winters: rescue from 1.0 gas -97/-106, hp30 -503, hp60 -50 min/day; from 1.8 nothing; excluding minimum-output hours removes worsening pairs (worst +100 -> 0 gas). Entry 170 recorded (Sergeant D 09:33: I + D, D never frozen). Round 7 (540 winters): CI+D = CI (89/90 same end coefficient), D window mean <= 0.024 C vs CI 0.74-2.25; PID integral (I, I+D) cannot rescue a too-low curve; guard 0.1 C/h no effect. Report docs/sat/sim-1519/round5b-round7.md; study4.py flags EXCLUDE_ATMIN / PID_D_HOLD / SIGNAL (defaults reproduce round 5 bit-exact, checked).

2026-10-31 ~11:00-12:15 UTC rounds 5c-5g (data docs/sat/sim-1519/data/round5c..5g_runs.json, scripts study5c/5d/5f, report sections 6-7). Robert asked in session whether the ADR becomes measure-hourly/decide-daily: answer Ask Sergeant -> Q-1519d re-ask first when Sergeant D is back (silent since 09:33). Q-1519e option B simulated: refuted (no gain, more churn), advice A. Optimum elsewhere: balance 2.4 from 1.8 ends 2.1-2.4, balance 1.6 ends 1.7. Blind spot: 2015/BENG at curve 1.0 -> PWM at minimum -> 0 clean hours, tuner holds (room 144-278 min/day out of band); one-sided exclusion (5d/5e) and a cold-day net (5f) rescue it but drift up on the plateau (selection bias) or run away in the light house (PID swing feedback, coefficient to 3.4); swing/cap guard (5g) removes harm and rescue alike. Queued Q-1519g. Round 5 reproduced bit-exact after every model change.

2026-10-31 ~12:20 UTC round 8 (Robert: another year, 2-day window): 2024-25 Open-Meteo, same houses/radiators (out of sample), rule as decided. 2025-26 runs reproduce round 5d 150/150. Cliff below ~1.2 + plateau hold, optima move 0.1-0.3. Rule safe: worst pair +2 gas / +8 hp30 / 0 hp60, flips <= 0.5. Rescue from 1.0 slower in the milder winter (gas median -29 vs -106, hp60 -2 vs -55) because fewer clean hours (hp60 25-30/winter). Decisions recorded: entry 172 (Q-1519d B, Sergeant D), 173 (Q-1519e A, Sergeant D + Robert), 174 (Q-1519g A + extra logging, Robert). Git frozen by session ea (TASK-1521 bump) - files edited, not yet committed: corrections 173/174, open-questions, report section 9, study8.py, round8_runs.json, weather_2024_25/2025_26.json.

2026-10-31 round 9 (Robert: twenty years of runs): 20 winters 2005-06..2024-25, 11,100 winters, 2-day and 3-day window, radiators from 2025-26 (out of sample); 2024-25 equals round 8 300/300. Worst pair over 20 winters +33 min/day (hp60 2 d), 0.9-4.3 % of pairs worse by > 5 min/day. Gain scales with cold (gas rescue from 1.0: -415 in 2009-11, ~0 in 2013-14/2019-20). Flips concentrate in hp30 near the optimum; 3 d halves them (88 -> 25 runs >= 3 flips, mean 0.31 -> 0.17) and lowers worst pairs (hp30 +28 -> +19, hp60 +33 -> +21) at a slower rescue (gas -33 -> -19, hp60 -5 -> 0). Q-1519h is a real trade-off; report section 9. Simulation paused once for session ea's alpha.569 build (2 workers during the build).

2026-10-31 rounds 10-11 + evaluation (Robert: critical evaluation vs OT AFS v1.1f low-load 2.1.1, KISS). Round 10: PID kept on a coefficient step (ADR-204 Must Not) changes -5..+2 min/day, 443/450 same end coefficient. Round 11: margin 2.0 vs 3.0 calibrated: gas 2.0 better (median -6..-13, 75-85 % pairs), hp30 neutral, hp60 +2-3; starts and fuel unchanged; hp60 ~27-33 starts/day (concern for heat pumps). AFS gap found: ID3 HB bit 4 (mandatory master check) read only for MQTT (OTGW-Core.ino:3186), SAT ignores it; thermo-nova too. Report docs/sat/sim-1519/evaluation-2026-10-31.md. ADR-205 created as Proposed skeleton (not yet filled/committed).
<!-- SECTION:NOTES:END -->
