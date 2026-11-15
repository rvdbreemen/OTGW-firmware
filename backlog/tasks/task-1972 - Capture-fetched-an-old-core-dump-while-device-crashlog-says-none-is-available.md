---
id: TASK-1972
title: Capture fetched an old core dump while device/crashlog says none is available
status: To Do
assignee: []
created_date: '2026-11-15T11:57:00Z'
labels:
  - capture
  - coredump
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1009000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (2026-11-15, TASK-1964 bench): the capture tool fetched a 31268-byte coredump-*.bin (ESP core dump header + ELF) from GET /api/v2/device/coredump while /api/v2/device/crashlog reported available:false. Probably an old dump that stays in the coredump partition after the crash it belongs to. Risk: someone analyses an old dump as a new crash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Find out from the code how device/crashlog decides available and how device/coredump decides to serve the partition; say which is right for an old dump (with file:line)
- [ ] #2 At minimum the capture labels a core dump with what tells its age (boot count / reset reason / app sha vs the running firmware) in the summary and the file name, so an old dump is not taken for a new crash; host test on the fetch
- [ ] #3 If the firmware should not serve or should flag a dump that belongs to an earlier crash: fix with a host test red/green, openapi updated
<!-- AC:END -->
