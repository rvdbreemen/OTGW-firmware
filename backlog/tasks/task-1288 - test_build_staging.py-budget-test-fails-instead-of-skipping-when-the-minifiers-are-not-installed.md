---
id: TASK-1288
title: >-
  test_build_staging.py budget test fails instead of skipping when the minifiers
  are not installed
status: To Do
assignee: []
created_date: '2026-10-22T03:52:00Z'
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
- [ ] #1 Without rjsmin/rcssmin the real-assets half of the budget test skips with a clear reason (the dummy-oversize half still runs); with the build venv it still runs and passes
- [ ] #2 Shown: host Python run gives OK (skipped) instead of an error; .build-venv run gives OK
<!-- AC:END -->
