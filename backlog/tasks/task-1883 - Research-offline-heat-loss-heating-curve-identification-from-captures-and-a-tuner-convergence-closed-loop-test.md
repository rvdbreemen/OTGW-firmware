---
id: TASK-1883
title: >-
  Research: offline heat-loss / heating-curve identification from captures and a
  tuner convergence closed-loop test
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T06:58:00Z'
labels:
  - sat
  - research
  - tests
dependencies:
  - TASK-1879
priority: low
ordinal: 956000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lessons 4 and 5 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). (4) VTherm fits dT = g(Tout-Tin) + B u + q per cycle bin with a 70/30 chronological split, physical guards, condition number < 1000, >= 40 cycles, block bootstrap, labelled exploratory and never auto-applied (tpi_identification.py:98-198, 597, 630-650). For us: estimate heat loss and the heating-curve coefficient from 24 h captures in the analyze-capture pipeline, as evidence for the coefficient question (entries 298-312). Compare at least two open-source system-identification candidates first (directive 2026-10-14). (5) Add a closed-loop test that runs the curve self-tune for weeks against a house model with a known correct coefficient and checks convergence without oscillation (counterpart of VTherm tests/test_auto_tpi_simulation.py:58-98); first check what tests/test_sat_sim_boiler_model.py already covers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Candidate comparison (>= 2 open-source options, licence, activity, fit) recorded in the task notes with a recommendation
- [x] #2 Prototype estimates the coefficient with a confidence interval on at least one existing long-run capture, labelled exploratory
- [x] #3 Tuner convergence test: fails on a mutant tuner that oscillates or diverges, passes on the real tuner
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 picked up by the comms session on Robert's request via the coordinator (no rush). Research runs in a background agent in wt-comms; output docs/sat/research/ + a test under tests/.

AC1 comparison (PyPI/CRAN/GitHub, 2026-11-13): numpy 2.5.4 (BSD-3, no deps); scipy 1.18.1 (BSD); statsmodels 0.15.0 (BSD-3; pandas, patsy, formulaic); arch 9.0.0 (NCSA, MovingBlockBootstrap, needs statsmodels); ctsmTMB 1.1.1 (R, GPL-3, CRAN 2026-07-14, continuous-time grey-box); locie/pySIP v1.0.0 2023-08-17 (no licence file found, pins numpy 1.x, pymc/numba; excluded); BuildingsPy 5.2.2 (simulation helper, not identification). PyPI 'pysip' is an unrelated SIP package. Choice: base on numpy (3-parameter lstsq + block bootstrap, ~30 lines; five transitive packages not worth it). Upgrade path: statsmodels + arch for HAC errors or non-linear emitters; ctsmTMB offline for a rigorous RC study. Metadata only for statsmodels/arch/ctsmTMB/pySIP, not run.

Evidence run by comms 2026-11-13 in wt-comms: python tests/test_sat_tuner_convergence.py -> Ran 5 tests, OK (exit 0); OTGW_TUNER_MUTANT=sign -> FAILED (failures=4, skipped=1), exit 1; OTGW_TUNER_MUTANT=step10 -> FAILED (failures=4, skipped=1), exit 1. Real scenarios: raise 1.0->1.8 final 1.70 (7 steps, 0 direction changes), lower 2.6->1.4 final 1.50 (11 steps, 0 direction changes), already-right 1.80 (0 steps). Fixture observation for routing, not a conclusion: with an evening gain pulse only, the real tuner stops at 1.50 vs c_true 1.8 with phases_disagree (warm-up +0.87, floating +1.35), consistent with ADR-228/230. Side finding: tests/test_sat_curve_selftune.py:898-899 runs the exe without SATTUNE_FILE/cwd and leaves sat_tune_test.json in the repo root (one is untracked in the main dev tree).

Open observation for later (not a conclusion): in the one-sided disturbance fixture (evening gain only) the real tuner settles at 1.50 against c_true 1.8 with reason phases_disagree (warm-up mean +0.87 holds, floating mean +1.35 says higher). Consistent with ADR-228/230 as recorded (entries 303-305, 313, 315). Possible single question for Sergeant D, to be considered only after Q-PWM1 and Q-1882a are answered (one question at a time).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: library comparison above (numpy chosen, statsmodels+arch upgrade path, ctsmTMB for offline RC study); report docs/sat/research/heat-loss-identification-2026-11-13.md. AC2 (EXPLORATORY): docs/sat/research/heat-loss-id/heat_loss_id.py on gruijter graph exports 1110+1111 (31.5 h, 114 cycles, 64 bins) and 1112pm (9.2 h): full model fails the 0<g<1 guard (no valid estimate); q=0 variant passes guards, c* at 5 C = 0.40, 95 % CI [-1.40, 2.57], hold-out RMSE 0.503 vs 0.538 zero model: the data cannot separate 1.12 from 1.8. 9.2 h is below the 40-cycle/40-bin minimum. Results JSON in heat-loss-id/results/. AC3: tests/test_sat_tuner_convergence.py drives the real satCurveTune blocks (extracted and compiled like test_sat_curve_selftune.py) in a 1R1C closed loop: OK on the real tuner, FAILED on mutants sign and step10 (runs above). Not verified: other libraries not run; no ground truth for the house; heat proxy assumed; one house/PI fixture/disturbance pattern; no firmware PID/PWM/DHW in the loop; nothing on a device.
<!-- SECTION:FINAL_SUMMARY:END -->
