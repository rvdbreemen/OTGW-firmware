---
id: TASK-1922
title: >-
  test_capture_otframes test_records_frames loses the first frame under parallel
  load (passes alone)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T05:44:00Z'
updated_date: '2026-11-14T12:41:00Z'
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
- [x] #1 The race is reproduced (e.g. by delaying the capture tool's connect) and shown to drop the first frame
- [x] #2 The test waits for an explicit ready signal instead of timing; 20 runs under run_tests.py -j 4 with load pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (09) root cause, not test timing: scripts/otgw_capture.py WebSocket.__init__ reads the upgrade with recv(4096) until CRLFCRLF and dropped every byte after the header. When the 101 response, the ping and the first frame arrive in one recv() (a loaded host reads late; a device can log at once), those frames were lost: the missing T10010A00 in the flaky runs. Reproduced deterministically by new test_frames_in_the_upgrade_read_are_kept: the capture's socket waits 0.3 s before its first recv(), so all the fake device's frames are buffered with the 101. Old code: 3/3 FAIL 'Lists differ: [] != [T10010A00, B40000800, R90190000, A401920B0, E00000000]' (all frames in that read dropped). Fix: the upgrade keeps the bytes after the header in self._pending and _read_exact() consumes them first. Fix: 3/3 OK (7 tests). The same WebSocket class serves the CDP browser capture (otgw_capture.py:1682), which gets the same fix; test_capture_tool 52 tests PASS on the fix.

AC2: instead of a ready handshake around a timing race, the test now forces the coalesced read and the tool keeps the bytes, so no timing is left to race. 20 runs of python scripts/run_tests.py -j 4 otframes capture_discovery telnet_setup flash_zip (otframes in parallel with three other capture/tool test files): 20/20 exit 0, test_capture_otframes PASS 5.2-5.8 s each (log: 09 scratchpad r1922-20runs.log).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: otgw_capture.py WebSocket upgrade dropped bytes received together with the 101 header, so the first frame(s) were lost when the host read late. AC1: deterministic reproduction (test_frames_in_the_upgrade_read_are_kept, delayed first recv) fails 3/3 on the old code with all buffered frames missing. AC2: the upgrade keeps the leftover bytes (self._pending, consumed first by _read_exact); the new test and test_records_frames pass 3/3 alone and 20/20 under run_tests.py -j 4 with three other test files in parallel; test_capture_tool 52 tests pass. No firmware change, so no build or bench. Not done: a 20-run red count on the old code under load (the deterministic test is the red proof).
<!-- SECTION:FINAL_SUMMARY:END -->
