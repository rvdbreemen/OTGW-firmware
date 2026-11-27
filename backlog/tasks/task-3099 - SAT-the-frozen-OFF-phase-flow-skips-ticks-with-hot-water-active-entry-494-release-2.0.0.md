---
id: TASK-3099
title: >-
  SAT: the frozen OFF-phase flow skips ticks with hot water active (entry 494,
  release 2.0.0)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:39:00Z'
updated_date: '2026-11-27T08:00:00Z'
labels: []
dependencies: []
ordinal: 1164000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D answered Q-444b with B (entry 494, read from 'Q446b: B', our interpretation): ticks with hot water active are skipped when we pick 'the last ON value'; the flow frozen for the PWM OFF phase (entry 446) comes from the last heating tick. Extends the known limit of TASK-2993. SATcontrol.ino:1711-1712 sets _pwm_lastOnFlowTemp on every ON-phase tick. NOTE: a tick with the DHW bit set never reaches satPwmSchedule() (the control loop returns early, SATcontrol.ino ~8241), so the literal condition bDhwActive is vacuous at :1711; the exact condition is in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan quotes thermo-nova (7beaaf5, opened this session) and Sergeant D entries 494, 444, 446, 437, 103, 105, 106 with agree or conflict per entry
- [x] #2 Host test on the real PWM block: a hot-water draw at the end of the ON phase gives the long pause on the old code (red) and the short pause on the fix (green), reaching the real functions
- [ ] #3 One prerelease bump per src commit; assert-head.sh before any committed message; no push before the coordinator's slot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Entry 499 (Sergeant D, Q-444c A, 2026-11-26, corrects 494): after a hot-water draw inside a PWM ON phase stop updating _pwm_lastOnFlowTemp for the rest of that ON phase (flag _pwm_dhwResumed, existing: set by satPwmDhwResume, cleared at the next ON start and satPwmReset), so the OFF phase freezes the flow of the last tick before the draw. Site SATcontrol.ino satPwmSchedule ~1711. thermo-nova 7beaaf5: held temperature moves by EMA only inside _effective_on_temperature_update_window after flame_on_at (pwm.py:144-149), duty reads it (pwm.py:229-239), so a late draw cannot move it there. Per entry: 499 followed, 494 corrected by 499, 444/446 agree, 437 agree (live flow in ON), 95/103 agree (clock paused, DHW-bit tick never reaches the schedule), 105/106 agree (hot-water flame mark unchanged).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-26 (host only, NOT verified on a unit): tests/test_sat_pwm_frozen_flow_dhw.py on the real PWM block with the control loop's DHW hooks: 8/8 green on the fix, 4/8 red on origin/dev (late draw: OFF pause 1016 s instead of 936 s, +80 s at 60 C, +104 s at 65 C); negative tests: no draw unchanged, draw before the first ON tick uses the live flow as today, next ON phase stores again, ON-phase duty keeps the live flow; mutants caught: literal bDhwActive (dead code, the old behaviour), flag never cleared, never update, freeze the live duty too. Other SAT host tests green (2993 live flow 17, on_freeze 25, hp_pwm 35, anchors 64, overshoot_sustain 93, start_limit 27, slave_lowload 20, tolerance_band 30); evaluate --quick 88/0. Commit e7a9f4cc2 local.

Known limits: (1) a draw that no control tick and no flame edge sees (starts and ends between two ticks, flame stays on) leaves no trace in the PWM block, so the first tick after it still stores the draw-heated flow (pinned by test_known_limit_a_draw_that_no_control_tick_sees_is_not_covered); coordinator decision: no live DHW-bit polling, documented only. (2) Open sub-question to Sergeant D: last tick BEFORE the draw (implemented, our reading, test_last_tick_before_the_draw_differs_from_the_last_clean_tick: 50 C) versus last clean tick (52 C); if he answers A2 the change is one flag. Push held until his answer is recorded.

2026-11-27 (otgw-webui): AC unit evidence = flashed + 5 h soak; simulation not discriminating (_comms/task3099_bench_result.md: the sim DHW draw also cools the sim room 19.94 -> 19.09 C in 300 s, PID jumps 30 -> 56.5, duty 1.00 after every tap, so no OFF phase). Fix is host-tested on the real PWM block, NOT verified old-vs-new on a unit; the AC for a real tap stays open. Robert chose A on question 17 (DM 1575733951773941821): ships in the 08:00 beta with this wording. Rebased on origin/dev 25875e928 as 2a6aea677; src diff against 9348c1a6f is 0 lines.

2026-11-27 otgw-docs check for AC3: beta.452 is published (tag v2.0.0-beta.452) and the release build soaked PASS on .63 (61 min) and the OTGW32 bench unit (60 min), see OTGW-field-data/_train/sat48rel.verdict.txt and sat48rel61.verdict.txt. AC3 says one prerelease bump per src commit: cfcddd034 (SATcontrol.ino, no version bump, banner still beta.451) and 2a6aea677 (banner beta.451 to beta.452) are two src commits with ONE bump, so AC3 is NOT met as written; the release image is beta.452 anyway. Left unticked. The change itself is verified by 10 host tests (python tests/test_sat_pwm_frozen_flow_dhw.py, rerun 2026-11-27, OK) and is NOT verified on a real hot-water tap (the simulation cannot separate old and new code).
<!-- SECTION:NOTES:END -->
