---
id: TASK-2041
title: >-
  test_refresh_storm gate-balance timeouts are not load-tolerant (fails under
  CPU load, stub-only)
status: To Do
assignee: []
created_date: '2026-11-19T04:27:00Z'
labels:
  - tests
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1047000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tests/test_refresh_storm.py::TestStorm::test_fixed_route_passes (arm1_w2) fails under machine load: 'gate balance FAIL: sequential 2/4 ok (3 needed a retry) bad=[(/, timeout), (/, timeout)] | pair pass'. Found 2026-11-19 in the full-suite reference run on origin/dev b72c6d6c5 (-j4) and again run alone (CPU 84 %, 5.8 of 27.9 GB free). The test exercises scripts/tests/refresh_storm.py against a local Python StubDevice (its docstring: proves nothing about the firmware); tool and test unchanged since 14d304af9 (2026-11-06); another arm in the same run passed 4/4 with 0 retries. TASK-1509 made the failed-connect cases deterministic under CPU load; the sequential gate-balance GETs still use a wall-clock timeout that load can exceed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A quiet-window solo run is recorded (pass/fail, CPU and free memory at that moment)
- [ ] #2 The gate-balance sequential GETs on the stub no longer fail on a timeout caused by host load (e.g. timeout scaled or retried as in TASK-1509), shown red on the old test under synthetic CPU load and green on the fix
- [ ] #3 The detectors still fire both ways (leaked REST slot fails the pair; clean stub passes)
<!-- AC:END -->
