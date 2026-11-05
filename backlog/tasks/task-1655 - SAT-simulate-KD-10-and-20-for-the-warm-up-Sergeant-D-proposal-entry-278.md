---
id: TASK-1655
title: >-
  SAT: simulate KD -10% and -20% for the warm-up (Sergeant D proposal, entry
  278)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T08:29:00Z'
updated_date: '2026-11-05T12:58:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 779000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-11-05 (entry 277/278, #dev-sat-mqtt 1567789991315120178 and 1567814060550918146): the slow warm-up comes from KD (D about -2.5 C while the room warms 0.5 C/h in our example), not from the integral, which must stay as is. Simulate KD x0.9 and x0.8 against today (kd = 0.07*8400*kp, pid.py:85-93, SATpid.ino:221) in the TASK-1412.16/1412.19 harness; bring the numbers to him (queued question Q-1412.16b) before any firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Simulation baseline vs KD x0.9 vs x0.8 per plant class: time to within 0.1/0.3 C, overshoot, starts per day, comfort error; thresholds pre-registered before the run
- [ ] #2 Report posted to Sergeant D with the numbers and a recommendation; his answer recorded as a numbered entry
- [ ] #3 Only if he agrees: firmware change + host test red/green + ADR or spec update
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 6c: pre-registration committed be3dd1726 (wt-6c-1655, base origin/dev 005d3073d) before any grid run: docs/sat/sim-1655/thresholds.md, scripts/sat_kd_warmup_sim_1655.py, tests/test_sat_kd_warmup_sim_1655.py (4/4 OK: copy reproduces committed 1412.19 very_low_load seed 2 row exactly; kd = round3(f*0.07*8400*kp) = 14112 / 12700.8 / 11289.6 at kp 24). Grid run (1728 runs, ~7 min) held: commit charge 87.9% >= 85%.

2026-11-05 6c: AC1 evidence. Pre-registration be3dd1726 committed before the first grid run (12:14-12:30, one process, one class at a time, commit charge 81.3-82.2% before each class). Results 8fe1bdd8d, corrections c28b56ef9 + 9b8287a65 (wt-6c-1655, base origin/dev 005d3073d, not pushed: beta.95 freeze). docs/sat/sim-1655/runs.json meta thresholds_commit be3dd1726; tests/test_sat_kd_warmup_sim_1655.py 4/4 OK. Validity check passed (baseline D in the approach -1.15..-2.85 C, radiator classes). x0.9 t03 gain 0.3-2.0 %, x0.8 0.4-4.0 %; pre-registered rule: keep KD (no 10 % gain in any class). Own x0.9 effects: G4 2015 heavy rad +0.8 min; worst-run starts 77->84 in 1983-92 light rad. 0.9f*0.07f vs 0.063f: 0 rounded-kd mismatches over kp 0.001..200. Report docs/sat/sim-1655/report.md with a draft question to Sergeant D. AC2 open (6b posts in #dev-sat-mqtt); AC3 is triggered by entry 279 and belongs to the firmware task still to be filed.

2026-11-05 6c: 6b posted the results in #dev-sat-mqtt (msg 1567864588442009651), numbers verbatim; recommendation C = keep 0.063 + study the minimum-power/PWM phase (not B), per Robert's 'best result, not parity' rule. This departs from the pre-registered outcome (keep KD) on purpose; recorded in report.md section 9 (local commit aa96eb53d). Firmware task for entry 279: TASK-1656 (6b lane). AC2 waits for Sergeant D's answer as a numbered entry. Commits stay local until 'beta.95 tag out'.

2026-11-05 ~14:00: after beta.95 tag out (6b), rebased onto origin/dev and pushed c8481993f..2e757aaa9. New shas: pre-registration be3dd1726 -> f6e3a10d2 (thresholds.md identical, git diff empty), results 02b50ee28, report a4e293150/7b81bcec8/30ef35937, sha mapping note 2e757aaa9. Test 4/4 OK after rebase. AC2 waits for Sergeant D.
<!-- SECTION:NOTES:END -->
