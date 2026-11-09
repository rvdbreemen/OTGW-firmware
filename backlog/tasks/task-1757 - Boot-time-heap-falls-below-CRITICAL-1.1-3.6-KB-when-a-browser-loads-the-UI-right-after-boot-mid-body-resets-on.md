---
id: TASK-1757
title: >-
  Boot-time heap falls below CRITICAL (1.1-3.6 KB) when a browser loads the UI
  right after boot; mid-body resets on '/'
status: To Do
assignee: []
created_date: '2026-11-09T17:05:00Z'
labels:
  - heap
  - bug
  - webui
  - 2.0.0-stable-gate
dependencies: []
priority: high
ordinal: 859000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 stable gate. Found in TASK-1528 step b / TASK-1675 R1 (bench .61, beta.162+5786928, 2026-11-09, 10 reboots, run-20261109). Clean boots stay at 22-33 KB in the boot window. With the two-tab trace (2 tabs, '/' then '/favicon.ico', from the first HTTP answer after boot, 120 s) the windowed heap minimum falls to 1.1-3.6 KB, below the CRITICAL tier (3072 B), and 3 requests for '/' were reset mid-body at boot +8, +19 and +20 s, in the low-heap boots. 0 hangs. A user who opens the web UI right after an update or reboot hits this window. Robert, 2026-11-09: TASK-1675 stays open; this task carries the fix. Related TASK-1675 and TASK-1528.

Fix idea for the plan only (not decided): hold back UI serving (e.g. 503 + Retry-After, or a small static wait page) until '[HEAP] boot done'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce with ONE normal browser tab loading the UI right after boot first (not the two-tab trace): windowed heap minimum per boot phase and any reset, on the current dev build
- [ ] #2 Name which allocations in the boot window combine with UI serving to reach below CRITICAL, with evidence
- [ ] #3 Fix agreed with Robert in the plan; after the fix the one-tab and two-tab boot runs stay above CRITICAL with 0 mid-body resets, with old-vs-fix numbers
<!-- AC:END -->
