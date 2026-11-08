---
id: TASK-1739
title: >-
  Web UI test harness leaks headless Chrome process trees on Windows
  (chrome.kill() does not kill children)
status: To Do
assignee: []
created_date: '2026-11-08T17:57:00Z'
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
- [ ] #1 Shared launcher kills the full Chrome tree on win32 and POSIX; no leftover chrome.exe after a run of the affected tests (process count before/after)
- [ ] #2 All webui tests use the shared launcher (grep shows no direct chrome.kill)
<!-- AC:END -->
