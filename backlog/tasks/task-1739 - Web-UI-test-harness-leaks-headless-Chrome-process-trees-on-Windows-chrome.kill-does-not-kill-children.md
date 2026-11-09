---
id: TASK-1739
title: >-
  Web UI test harness leaks headless Chrome process trees on Windows
  (chrome.kill() does not kill children)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T17:57:00Z'
updated_date: '2026-11-09T05:10:00Z'
labels:
  - tests
  - windows
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 845000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw-firmware-54 2026-11-08 ~19:00: leftover headless Chrome trees (28 processes) after sat-settings-locked-when-off and ble-nopsram-warning runs; chrome.kill() on Windows only ends the parent. At 88% commit the leaked trees add to the memory pressure that got the beta.150 build reaped and cause 20-47 s Chrome stalls in later tests (TASK-1731 AC2). Fix in tests/webui/lib (shared launcher): kill the whole tree (taskkill /T /F /PID on win32, or a process group on POSIX) in finally/exit handlers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All webui tests use the shared launcher (grep shows no direct chrome.kill)
- [x] #2 Shared launcher kills the full Chrome tree on win32; no leftover chrome.exe after a run of the affected tests (process count before/after)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08: the intermittent settings-reboot-badge failures are a separate test-side race (badge read before it is rendered), filed as TASK-1742 with the bisect numbers; not part of the Chrome process tree leak.
2026-11-08 implementation (lane-1739, wt-1723): tests/webui/lib/page-harness.mjs exports stopChromeTree(child) (Windows: taskkill /PID <pid> /T /F, then child.kill()) and guardChrome(child) (process 'exit' hook that stops the tree when a test throws or exits early); the harness close() and its spawn use them, and the 13 standalone Chrome tests import both and replace chrome.kill() (otd-mode-running-note gets the same fix with TASK-1731 on lane-1689b). node --check OK on all files. Evidence so far: origin/dev tab-strip-no-vscroll on a clean pass leaves no Chrome behind (12 -> 12 test processes), so the leak happens on stalled or killed runs, which guardChrome covers. NOT verified yet: the run of the new version was stopped by the Claude Code memory reaper at 89% commit before it reported. Red/green proof (kill a run mid-way: the old version leaves its tree, the new one does not) and a pass of the 13 tests go to tonight's quiet window.

2026-11-09 otgw-firmware-80: AC2: otd-mode-running-note.test.mjs (the last test with its own launcher stop) now imports stopChromeTree/guardChrome from tests/webui/lib/page-harness.mjs (guardChrome also stops the tree on an early exit). grep -rn 'chrome.kill|child.kill' tests/webui --include=*.mjs outside lib/page-harness.mjs: 0 hits. AC1 Windows half: chrome.exe count before/after a run, 0/0 for sat-settings-locked-when-off (6/6), ble-nopsram-warning (5/5), otd-mode-running-note (1/1), settings-reboot-badge (1/1). AC1 POSIX half: stopChromeTree on POSIX only calls child.kill() (no process-group kill) and there is no POSIX host or CI here to evidence it; needs Robert's decision to narrow AC1 to Windows or to add a detached process-group kill without evidence.

2026-11-09 Robert (in session otgw-firmware-80, AskUserQuestion): 'Narrow AC to Windows': AC1 reworded to win32 (now AC #2); the POSIX process-group kill is filed as its own low-priority task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 (all webui tests use the shared launcher): otd-mode-running-note now imports stopChromeTree/guardChrome; grep for chrome.kill/child.kill outside lib/page-harness.mjs gives 0 hits (commit d239caf51). AC2 (win32 tree kill, narrowed by Robert 2026-11-09): chrome.exe count 0 before and 0 after runs of sat-settings-locked-when-off 6/6, ble-nopsram-warning 5/5, otd-mode-running-note 1/1, settings-reboot-badge 1/1. POSIX half split into its own task; not verified (no POSIX host).
<!-- SECTION:FINAL_SUMMARY:END -->
