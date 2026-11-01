---
id: TASK-1509
title: >-
  tests/test_refresh_storm.py is flaky: TestProbeRecord expects 'refused' but
  gets 'timeout' under load
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T10:02:00Z'
updated_date: '2026-11-01T01:24:00Z'
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
- [x] #1 Root cause named (load-dependent connect timeout vs refused) and the test made deterministic (e.g. bind+close a port so connect is refused fast, or accept both classes where the test only means 'failed connect')
- [x] #2 10 consecutive runs green, also with a parallel CPU load
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root causes: (1) a refused loopback connect takes ~2 s on Windows (measured 2.02-2.05 s, the SYN is retried after the RST); under CPU load it passes the 5 s connect timeout and classifies as 'timeout'. test_probe_record_survives_a_failed_connect and test_probe_loop_keeps_probing_after_a_failed_connect are about a failed connect, so they accept refused or timeout. (2) test_big_window_cannot_prove_overlap (found while proving AC2): under load the stub can still be writing A's 60 KB when B is done, so A is really incomplete; the test now retries the real _peek_now until A reports complete (max 10 s), which is its precondition. Mutation check: a peek that returns one segment (recv 1460) still fails the test; the real peek passes.
Evidence: 10/10 runs of tests/test_refresh_storm.py green with every core loaded by a CPU burner (scratchpad rs1509c); before the second fix 9/10 (big_window failed once).
<!-- SECTION:FINAL_SUMMARY:END -->
