---
id: TASK-672
title: 'feat-2.0.0: port TASK-671 — remove residual delay(10) and dead executeCommand'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 11:25'
updated_date: '2026-05-22 11:26'
labels:
  - responsiveness
  - refactor
  - mainloop
  - otgw-2.0.0
dependencies: []
priority: high
ordinal: 52000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of dev TASK-671. The 2.0.0 line already has the drain-loop caps from TASK-651 (kMaxLinesPerDrain=4 in handlePICSerial and handleOTDirectBridgeStream), but two violations of the non-blocking-mainloop principle remain that match dev:

- A1: delay(10) in emergencyHeapRecovery() (helperStuff.ino:1083) — called from doBackgroundTasks() under HEAP_CRITICAL; yield() above is sufficient.
- B: executeCommand() (OTGW-Core.ino:837 #if HAS_PIC + 941 #else stub) — zero callers (verified via grep across src/). Contains two busy-wait while-loops (lines 866/877 in the HAS_PIC version) that would re-introduce synchronous PIC send-and-wait if revived.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A1: delay(10) removed from emergencyHeapRecovery() in helperStuff.ino:1083; yield() retained
- [ ] #2 B: executeCommand() removed from OTGW-Core.ino (both HAS_PIC and !HAS_PIC variants); grep src/ confirms no callers
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick reports no new failures
<!-- AC:END -->
