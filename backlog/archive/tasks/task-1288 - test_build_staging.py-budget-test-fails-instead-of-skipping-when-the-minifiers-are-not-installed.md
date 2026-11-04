---
id: TASK-1288
title: >-
  test_build_staging.py budget test fails instead of skipping when the minifiers
  are not installed
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T03:52:00Z'
updated_date: '2026-10-22T06:58:00Z'
labels:
  - test
  - build
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 416000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-22 while running every tests/test_*.py with the host Python (no rjsmin/rcssmin): TestLittleFsBudget.test_oversize_dummy_fails_and_real_assets_pass errors with SystemExit 1 ('assets need 768 KiB + 96 KiB reserve = 864 KiB, partition is 768 KiB') because without the minifiers the real assets are staged unminified. The seven TestStagingOnRealData tests skip in that case ('rjsmin/rcssmin not installed'); this one does not. With .build-venv (minifiers installed) the suite is OK: 640 KiB + 96 KiB <= 768 KiB. A false red in a plain `python tests/...` run hides real failures and costs a detour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Without rjsmin/rcssmin the real-assets half of the budget test skips with a clear reason (the dummy-oversize half still runs); with the build venv it still runs and passes
- [x] #2 Shown: host Python run gives OK (skipped) instead of an error; .build-venv run gives OK
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
tests/test_build_staging.py: the budget test is split. test_oversize_dummy_fails keeps running everywhere (needs only mklittlefs); test_real_assets_pass_the_budget skips without rjsmin/rcssmin with the reason 'the budget only holds for minified assets, run from .build-venv', because unminified assets legitimately exceed the 768 KiB partition (ADR-190 ships minified). Evidence, this session: host Python 'Ran 16 tests, OK (skipped=8)' (was 'FAILED (errors=1, skipped=7)'); .build-venv 'Ran 16 tests, OK' with 'LittleFS budget OK: 640 KiB assets + 96 KiB reserve <= 768 KiB'. No firmware change.
<!-- SECTION:FINAL_SUMMARY:END -->
