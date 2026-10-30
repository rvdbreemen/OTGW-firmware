---
id: TASK-1509
title: >-
  tests/test_refresh_storm.py is flaky: TestProbeRecord expects 'refused' but
  gets 'timeout' under load
status: To Do
assignee: []
created_date: '2026-10-30T10:02:00Z'
labels:
  - tests tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 665000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-30 by the coordinator: on a clean origin/dev worktree (6e02216bf) two consecutive runs gave run 1 OK (39 tests, skipped=2) and run 2 FAILED (failures=2): test_probe_record_survives_a_failed_connect (line 624) and test_probe_loop_keeps_probing_after_a_failed_connect (line 658) assert class 'refused' but get 'timeout'. The machine was busy (a firmware build and several agents). Likely the test's refused-connect target times out under load instead of refusing. Outputs: scratchpad refresh_run1.txt / refresh_run2.txt.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause named (load-dependent connect timeout vs refused) and the test made deterministic (e.g. bind+close a port so connect is refused fast, or accept both classes where the test only means 'failed connect')
- [ ] #2 10 consecutive runs green, also with a parallel CPU load
<!-- AC:END -->
