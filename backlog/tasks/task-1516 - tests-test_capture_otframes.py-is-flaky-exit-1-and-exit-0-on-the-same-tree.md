---
id: TASK-1516
title: 'tests/test_capture_otframes.py is flaky: exit 1 and exit 0 on the same tree'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T19:22:00Z'
updated_date: '2026-11-01T00:02:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 672000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-10-30 by the TASK-1506 lane: tests/test_capture_otframes.py exited 1 in a full one-at-a-time run, 0 on a rerun on base, 0 on a rerun with the change, 1 again later with the change; it touches none of the changed files. Same class as TASK-1509 (test_refresh_storm flaky under load). Find the timing/ordering dependency and make it deterministic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause named (file:line)
- [x] #2 20 consecutive runs exit 0, also while another test file runs in parallel
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause: tests/test_capture_otframes.py:139 ran the capture for a fixed 1.5 s / 1.0 s, so the outcome depended on machine load: the WebSocket handshake plus 9 messages may not finish in 1.5 s, and a refused loopback connect takes ~2 s on Windows (measured 2.02-2.05 s), longer than the 1.0 s window of test_degrades_when_unreachable. run_capture now waits for the outcome it checks (5 frames in the log / the otframes error logged), deadline 15 s.
Evidence: 20/20 runs exit 0 with tests/test_sat_hp_pwm.py running in parallel and all cores loaded by a CPU burner; a single run takes ~4 s.
<!-- SECTION:FINAL_SUMMARY:END -->
