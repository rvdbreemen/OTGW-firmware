---
id: TASK-3101
title: >-
  Replay fixtures and recipe for the TASK-3099 bench: hot-water tap at the end
  of the PWM ON phase (scenarios A, B, C)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T07:17:00Z'
updated_date: '2026-11-26T07:21:00Z'
labels: []
dependencies: []
ordinal: 1164000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator dispatch 08:15. scripts/gen_sat_pwm_tap_replay.py writes three replay files (A: tap with the flame staying on, B: tap then flame out, cooling flow 60 to 54 in the post-draw wait and re-ignition, C: no tap control) in the style of gen_sat_pwm_replay.py; a host test feeds each fixture to the real PWM block (tests/test_sat_pwm_frozen_flow_dhw.py harness) and checks the OFF pause: 936 s on the fix, longer on the old code for A and B, 936 s for C. Delivered to the bench-tester with sha256 and a one-page run recipe. Tooling only, nothing firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Generator writes A, B, C fixtures deterministically; sha256 listed
- [x] #2 Host test feeds each fixture to the real PWM block: A and B red on old origin/dev (OFF pause longer), green on the fix; C equal on both
- [x] #3 One-page run recipe with the SAT settings and how to read pwm_duty and the OFF length from sat/status; coordinated with the bench-tester
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-11-26 (otgw-webui), host only, NOT verified on a unit: OFF pause from tests/test_sat_pwm_tap_replay.py (real PWM block, ON starts at replay second 0): C 936.0 s; with the fix (lane commit 4e3940646) A and B 936.0 s; origin/dev A 1011.9 s, B 966.7 s; MUTATION/old fix commit e7a9f4cc2 (before the post-draw-wait fix F1) A 936.0 s but B 975.4 s, so the flame-out fixture B catches the leak the architect found; the test goes green on both sides (defect tests without 'Entry 499' in SATcontrol.ino, equal-pause tests with it). Fixtures A 0960c39a6bf304519ee2fd097bb096f64c7746c08c444b7b215367aa06ccedf7, B 737a3d56937ae1895fabd99dd6deace9bf58bf82b5124a0df4234acc71aa8cf5, C 7643eebcafa743a8c7f9fb579588705b8fde0f3831aa96ffe65af89a1eca2353 (bench-tester checked sha256sum -c OK). Limits: open-loop replay does not fix the second at which the unit ON phase starts (recipe: shift and restart); settings for 3 cycles/h and duty 0.22 are the bench-tester's to find.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 generator deterministic, sha256 above (scripts/gen_sat_pwm_tap_replay.py). AC2 host test: A and B red on origin/dev (OFF pause 1011.9 s and 966.7 s versus 936.0 s), green on the fix; C equal on both; B also red on the first fix commit e7a9f4cc2 (975.4 s). AC3 RECIPE.md (one page: settings, fields pwm_duty and pwm_phases, OFF length derivation, alignment, pass/fail) delivered to the bench-tester with sha256, who confirmed receipt and will pick the settings on the unit. Host-verified; the unit run (22:00-23:30 window, .63) is the bench-tester's.
<!-- SECTION:FINAL_SUMMARY:END -->
