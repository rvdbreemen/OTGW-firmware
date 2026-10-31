---
id: TASK-1519
title: >-
  SAT: design and plan an hourly heating-curve self-tune with Sergeant D
  (opt-in, logged); revisits entry 72
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-31T05:53:00Z'
updated_date: '2026-10-31T07:32:00Z'
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
<!-- SECTION:NOTES:END -->
