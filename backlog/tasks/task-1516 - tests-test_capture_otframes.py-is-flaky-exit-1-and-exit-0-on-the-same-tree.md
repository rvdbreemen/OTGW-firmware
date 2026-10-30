---
id: TASK-1516
title: 'tests/test_capture_otframes.py is flaky: exit 1 and exit 0 on the same tree'
status: To Do
assignee: []
created_date: '2026-10-30T19:22:00Z'
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
- [ ] #1 Root cause named (file:line)
- [ ] #2 20 consecutive runs exit 0, also while another test file runs in parallel
<!-- AC:END -->
