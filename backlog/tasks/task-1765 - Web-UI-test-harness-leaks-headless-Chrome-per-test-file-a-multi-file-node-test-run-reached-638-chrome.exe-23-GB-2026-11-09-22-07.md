---
id: TASK-1765
title: >-
  Web UI test harness leaks headless Chrome per test file; a multi-file node
  --test run reached 638 chrome.exe / 23 GB (2026-11-09 22:07)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T21:22:00Z'
updated_date: '2026-11-09T21:55:00Z'
labels:
  - tests
  - webui
  - tooling
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 863000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-11-09 22:07: one node --test run over all tests/webui files (TASK-1764 workflow) left 638 chrome.exe with otgw-cdp-* profiles; commit charge reached 90 % and other sessions' builds are at risk (an earlier release build was killed on low memory). The launcher processes were gone, so each Chrome tree outlived its test file. Fix: (1) the harness kills its own Chrome process tree on every exit path (after, error, timeout, SIGINT), building on the TASK-1739 helper; (2) a guard: more than one webui file in one node --test run refuses to start, or runs with concurrency 1, unless explicitly overridden. Leftover empty Temp profile dirs are out of scope (coordinator puts that to Robert).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Red: a run of 3 webui files on the old harness, with one file aborted, leaves chrome.exe behind (count before and after)
- [ ] #2 The harness kills its own Chrome tree on after, error, timeout and SIGINT; green: same 3-file run leaves 0 new chrome.exe
- [x] #3 Guard: a multi-file node --test run over tests/webui refuses or runs with concurrency 1 unless an explicit override is set; shown with output
- [x] #4 scripts/run_tests.py and the full webui suite still pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 22:3x, kill test (scratchpad t1765/redgreen.ps1; 3 files v2-navigation, sat-page-order, theme-toggle-labels in ONE node --test run, node processes hard-killed after 8 s, private TEMP so only this run's Chromes count): old harness 54 chrome.exe during, 0 at +2 s after the kill; new 19 during, 0 at +2 s. A hard kill of node does NOT orphan Chrome on this path (hypothesis, not verified: libuv puts non-detached children in a kill-on-job-close job). Two earlier runs in the shared %TEMP% (23 and 18 left at +6 s, gone later) are confounded by Robert's otgw-cdp cleanup and do not count. So AC1 red is not reproduced; the 22:07 orphans came through another exit path (asked the coordinator how the TASK-1764 run was started and ended). A detached reaper was written and removed again: no red for it. Kept: exit + SIGINT/SIGTERM/SIGHUP handlers on the TASK-1739 taskkill helper, and a machine-wide Chrome slot cap (launchChrome, default 1, OTGW_WEBUI_MAX_CHROME; run_tests.py sets it to its -j; slot wait 300 s < 600 s per-file timeout). All 14 own-spawn test files plus page-harness go through launchChrome.

Cap red/green (scratchpad t1765/peak.ps1; 3 files v2-navigation, sat-page-order, theme-toggle-labels in ONE node --test run to completion, private TEMP, sampled every second): OLD harness peak 3 browser processes / 67 chrome.exe; NEW peak 1 browser / 16 chrome.exe; 0 left after both. 80's facts on the 22:07 run (TASK-1764 workflow subagent, Git Bash): three overlapping 'timeout N node --test tests/webui/...' runs, all files in parallel, kept running orphaned after a TaskStop of the workflow (431 otgw-cdp chrome at that point); Stop-Process on the orphaned timeout.exe/node processes cleared all Chrome within seconds. So the count came from concurrency of orphaned parallel runs, not from Chrome surviving a kill; the machine-wide cap limits even orphaned runs to 1 test Chrome (OTGW_WEBUI_MAX_CHROME to raise). AC4: scripts/run_tests.py --webui --no-host -j 2 .test.mjs: files 55 passed 55 failed 0 timeout 0 (464.9 s). AC1/AC2 stay open: a hard kill did not leak on this machine (old harness 0 left at +2 s), so there is no red for a kill-path fix; the exit/signal handlers are kept on the TASK-1739 helper but unproven. Proposal: reword AC1/AC2 to the concurrency cause or drop them (decision for Robert/coordinator). Flake seen on the way: TASK-1766 (v2-navigation, 1 in 3 also on the old harness).
<!-- SECTION:NOTES:END -->
