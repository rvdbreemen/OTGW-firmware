---
id: TASK-1865
title: >-
  Runtime task watchdog in async_tcp on beta.182 (gruijter 2026-11-12 01:08:50,
  also 2026-11-11 18:46:31)
status: To Do
assignee: []
created_date: '2026-11-12T11:51:00Z'
labels:
  - bug
  - crash
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 941000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's overnight capture (#alpha-testing msg https://discord.com/channels/812969634638725140/1514720723980259460/1570395420667023420, upload 20261111-213113-upload.zip; analysis by the coordinator's /analyze-capture agent). At about 01:08 the gateway ESP rebooted on a Task watchdog crash in async_tcp (reboot #96, beta.182); same pattern on 2026-11-11 18:46:31 on the previous build. Core dump: pc=0x4037627a, elf=ebfbbaf5; the zip holds coredump-20261111-213113.bin with a matching app SHA. Side effect seen: after the reboot SAT ran one tick before the first room temperature (curve only, CS=30.1 + CH enable for 22 s, burner did not light). Different from TASK-1763 (ipc1 boot panic) and TASK-1789 (setup TWDT in updateRebootLog). A crash, so no needs-maintainer-ok.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The core dump coredump-20261111-213113.bin is decoded with the beta.182 .elf (app SHA ebfbbaf5) and the backtrace of the async_tcp watchdog is recorded
- [ ] #2 The backtrace is compared with the current dev code: the cause is named with file:line, or shown to be fixed already by a later commit
- [ ] #3 Fix with a reproduction red on the old code and green on the fix, or close as fixed with the evidence of the commit that fixed it
<!-- AC:END -->
