---
id: TASK-672
title: 'feat-2.0.0: port TASK-671 — remove residual delay(10) and dead executeCommand'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 11:25'
updated_date: '2026-05-22 12:27'
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
- [x] #1 A1: delay(10) removed from emergencyHeapRecovery() in helperStuff.ino:1083; yield() retained
- [x] #2 B: executeCommand() removed from OTGW-Core.ino (both HAS_PIC and !HAS_PIC variants); grep src/ confirms no callers
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick reports no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the dev TASK-671 residuals to the 2.0.0 line via PR #627 (merged). A2/A3 from the master plan were already in place on this branch (kMaxLinesPerDrain=4 from TASK-651); only A1 + B needed action here.

Changes:
- A1 helperStuff.ino: removed delay(10) from emergencyHeapRecovery() at line 1083. Same rationale as dev: yield() alone is enough; must stay non-blocking from doBackgroundTasks().
- B  OTGW-Core.ino: deleted both the #if HAS_PIC implementation of executeCommand() (lines 837-935) and the #else stub (lines 937-940). Zero callers confirmed via grep across src/.

Tests:
- python build.py --firmware --target esp8266: exit 0 (esp32 deferred to CI, same constraint as TASK-651)
- python evaluate.py --quick: 60/68 passed, 1 pre-existing warning, 0 failures (Health 98.5%, unchanged baseline)

Operational note: commit was created with -c commit.gpgsign=false because the sandbox code-signing helper rejects commits from alternate worktrees (400 missing source). Dev commits are also effectively unsigned in this environment, so operationally equivalent. User explicitly authorized the bypass.
<!-- SECTION:FINAL_SUMMARY:END -->
