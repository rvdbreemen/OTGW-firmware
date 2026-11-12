---
id: TASK-1878
title: >-
  tests/test_sat_curve_selftune.py leaves sat_tune_test.json in the repo root
  after a test run
status: To Do
assignee: []
created_date: '2026-11-12T22:07:00Z'
labels:
  - tests
  - tooling
dependencies: []
priority: low
ordinal: 947000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1872 full run (2026-11-12): an untracked sat_tune_test.json appears in the repo root of every tree after the full tests folder (seen in wt-ba-1757, wt-ba-1872 and the main dev tree). Cause (read from the code, not yet reproduced in isolation): the compiled harness in tests/test_sat_curve_selftune.py uses g_tuneFile() (line 85) = getenv(SATTUNE_FILE) or the relative default sat_tune_test.json; the run at line 294-300 sets SATTUNE_FILE to a temp file, but the run at line 899 starts the exe without env and without cwd, so the file lands in the runner's cwd (scripts/run_tests.py runs every test with cwd=ROOT). Fix: pass SATTUNE_FILE into a TemporaryDirectory for that run too (or run it with cwd in the temp dir). Tests only, no firmware change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After python scripts/run_tests.py -j 1 test_sat_curve_selftune.py in a clean tree, git status shows no sat_tune_test.json, and the same check on the old code shows the file appearing
- [ ] #2 test_sat_curve_selftune.py stays green
<!-- AC:END -->
