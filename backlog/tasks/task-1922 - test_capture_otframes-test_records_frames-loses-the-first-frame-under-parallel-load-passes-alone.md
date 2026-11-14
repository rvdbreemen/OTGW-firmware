---
id: TASK-1922
title: >-
  test_capture_otframes test_records_frames loses the first frame under parallel
  load (passes alone)
status: To Do
assignee: []
created_date: '2026-11-14T05:44:00Z'
labels:
  - test
  - flaky
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 986000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen twice on 2026-11-14 (ba lane, TASK-1774 test runs), not related to that change (it does not touch the capture tooling). 1) Full scripts/run_tests.py, -j 4, under memory pressure (that run was later stopped by Claude Code for low memory): tests/test_capture_otframes.py FAILED 17.8 s, test_records_frames at tests/test_capture_otframes.py:162: AssertionError: Lists differ: ['B40000800', 'R90190000', 'A401920B0', 'E00000000'] != ['T10010A00', 'B40000800', 'R90190000', 'A401920B0', 'E00000000'], plus ResourceWarning unclosed sockets from scripts/otgw_capture.py. 2) run_tests.py -j 1 on four files while a build was about to start: 1 of 6 failed (case not captured). 3) Alone (python tests/test_capture_otframes.py): OK on wt-ba-1774 and on an origin-based lane. Reading (not verified): the first frame is lost, which fits a start-up race where the fake device sends T10010A00 before otgw_capture.py's connection or reader is ready; the test likely needs a ready handshake instead of timing. Same class as TASK-1854 (fails under -j 4, passes alone).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The race is reproduced (e.g. by delaying the capture tool's connect) and shown to drop the first frame
- [ ] #2 The test waits for an explicit ready signal instead of timing; 20 runs under run_tests.py -j 4 with load pass
<!-- AC:END -->
