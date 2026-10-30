---
id: TASK-1499
title: >-
  test_settings_float_roundtrip fails on dev: SATtargettemp has no clamp range
  the test can find (since TASK-1487?)
status: Done
assignee: []
created_date: '2026-10-30T05:35:00Z'
updated_date: '2026-10-30T18:07:00Z'
labels:
  - tests sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 655000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-30 by the coordinator while running the full tests folder for the alpha.548 batch. tests/test_settings_float_roundtrip.py fails on origin/dev (84df919e0) as well, so it predates the batch: 'SATtargettemp: no clamp range found in updateSetting()' (test line 181). settingStuff.ino:322 writes satUserTargetTemp() since TASK-1487 (alpha.537); the update path is at settingStuff.ino:998. Unverified which side is wrong (the test's pattern or a missing clamp).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause named: test pattern out of date, or SATtargettemp really lacks its range clamp in updateSetting()
- [x] #2 tests/test_settings_float_roundtrip.py exits 0 on dev, and a clamp regression (if that is the cause) is shown red-then-green
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause: the test pattern was out of date, the firmware is fine. SATtargettemp is clamped (settingStuff.ino:1046-1052, clampSettingFloat 5..30) inside a multi-line block since TASK-1487; the test only matched the one-line form. Fixing that exposed SATmaxsetpoint, clamped via parseMaxTSetUser() (settingStuff.ino:751, TASK-1373). Change: tests/test_settings_float_roundtrip.py reads block-form entries and recognises parseMaxTSetUser (0..SAT_MAXTSET_MAX). Evidence: old test FAILED (SATtargettemp: no clamp range found); new test OK, exit 0 (re-run by the coordinator); guard: scratch copies with the SATtargettemp block clamp, the SATcoefficient one-line clamp, or the parseMaxTSetUser call removed each FAIL; full suite 71/71 exit 0; evaluate EXIT 0. No firmware change.
<!-- SECTION:FINAL_SUMMARY:END -->
