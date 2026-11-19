---
id: TASK-2915
title: >-
  docs: SAT one PWM entry per burn, continuous until flame-out, fail-safe 3.0 C
  (entries 428-430) in the manual and the stable notes
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T12:53:00Z'
updated_date: '2026-11-19T14:09:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1054000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator 2026-11-19: prepare locally from Sergeant D entries 428/429/430 (origin a755055e5); push only via the coordinator slot after 392, once d3's implementation is on origin. Wording checked against d3's code and test names at landing; beta number filled in then. Also remove every 3.5 C fail-safe statement from the manual.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch05/h05 auto-switch and hourly-limit text match entries 428-430, citing them
- [x] #2 RELEASE_NOTES_2.0.0-draft.md SAT Changes bullet, 3.5 C line updated
- [x] #3 grep: no 3.5 C fail-safe left in docs/manuals, docs/api, README (MANUAL.md rebuilt)
- [x] #4 Wording compared with d3's commit on origin (code and test names) and the beta number filled in before the push
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 prepared locally on origin 0f788017e: ch05:192 / h05:217 (switch margin at most once per burn, continuous until flame-out, fail-safe 3.0 C / 30 s, only the first PWM ON of a burn counts; entries 428/429/430, 359, 89) and the draft SAT bullet + the 3.5 C line of :160. build_manual rebuilt, --check matches. grep 3[.,]5 C over docs/manuals, docs/MANUAL.md, docs/api, README: only the history clause '3.5 C before beta.TBD-2005'. Placeholder beta.TBD-2005 and task id TASK-2005 in the draft bullet to be replaced with d3's beta and task at landing (AC4). Not mine, flagged: docs/sat/firmware-sat-spec.md still says 3.5 C (describes the firmware; d3's lane).

AC4 2026-11-19: compared with d3's 40f511a8a (beta.391, task 2913). Code: SATcycles.ino SAT_OVERSHOOT_FAILSAFE_DELTA_C 3.5f -> 3.0f; _cycle_burnLeftPwm set on the underheat exit (flame on) and on the saturation exit only when satIsFlameOn(); satCycleCheckOvershootSwitch clears the switch-rule start while it is set, fail-safe still applies; cleared by satCycleNoteBurnEnd() from satCycleOnFlameChange() flame-off edge. Count: satCycleCountPwmStartThisFlame(flame) from satPwmBeginOn() and satPwmCountOnWhenChAllowed(): PWM ON with flame off always counts, with flame on only when none counted this flame. Tests: test_sat_overshoot_sustain.py class OnePwmEntryPerFlame, test_sat_pwm_start_limit.py::test_one_counted_pwm_start_per_flame. Wording fixed to the code: 'back to continuous while the flame burns' (EN/NL manual) and the notes bullet rephrased (switch margin no longer moves a burn that left PWM; only the safety switch can). beta.TBD-2005 -> beta.391, TASK-2005 -> TASK-2913 in the notes bullet. Gates: build_manual --check 'docs/MANUAL.md matches the chapters' exit 0; test_manual_generated + test_manual_sat_commands 10 passed; grep TBD (excl. TBD-2023 of another task) empty; no em dashes.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: ch05:192 / h05:217 describe one PWM entry per burn after a PWM exit with the flame on, the 3.0 C / 30 s fail-safe as the only way back, and one counted PWM ON per burn, citing entries 428, 429, 430, 359, 89 and TASK-2913, from beta.391. AC2: draft notes SAT bullet (entries 428 to 430, TASK-2913, beta.391) and the 2.0 C/60 s line now says 3.5 C, from beta.391 3.0 C. AC3: grep for the 3.5 C fail-safe over docs/manuals, docs/MANUAL.md, docs/api, README leaves only the history clause '3.5 C before beta.391'; MANUAL.md rebuilt, build_manual --check exit 0. AC4: compared with 40f511a8a (SATcycles.ino SAT_OVERSHOOT_FAILSAFE_DELTA_C 3.0f, _cycle_burnLeftPwm, satCycleNoteBurnEnd, satCycleCountPwmStartThisFlame; tests OnePwmEntryPerFlame, test_one_counted_pwm_start_per_flame); wording aligned. Tests: test_manual_generated + test_manual_sat_commands 10 passed. Not verified: the firmware behaviour itself (d3's task 2913 and its bench run).
<!-- SECTION:FINAL_SUMMARY:END -->
