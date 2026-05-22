---
id: TASK-671
title: >-
  Mainloop responsiveness: remove blocking delay(10) and bound handleOTGW drain
  loops; delete dead executeCommand
status: To Do
assignee: []
created_date: '2026-05-22 11:20'
labels:
  - responsiveness
  - refactor
  - mainloop
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit found 4 mainloop-responsiveness violations of the project's non-blocking-by-default architecture:

- A1: delay(10) in emergencyHeapRecovery() (helperStuff.ino:1135) — called from doBackgroundTasks() when heap is HEAP_CRITICAL. yield() on the line above is already sufficient.
- A2: while(OTGWSerial.available()) byte-loop in handleOTGW() (OTGW-Core.ino:4434-4476) drains the entire UART RX buffer per call without bound. PIC bursts/boot can produce 10-50ms stalls of MQTT/HTTP/WS.
- A3: while(OTGWstream.available()) byte-loop in handleOTGW() (OTGW-Core.ino:4480-4537) drains entire network buffer per call; large ser2net echo storms cause same stall.
- B: executeCommand() in OTGW-Core.ino:864 contains two busy-wait while-loops (lines 895/906) and has zero callers (verified via grep). Dead code with synchronous blocking pattern that should not be allowed to re-emerge.

Same scope is mirrored to a sibling 2.0.0 task that ports the fix where the code matches.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A1: delay(10) removed from emergencyHeapRecovery(); single yield() retained
- [ ] #2 A2: handleOTGW serial drain loop bounded to max 4 complete lines per call
- [ ] #3 A3: handleOTGW network drain loop bounded to max 4 complete commands per call
- [ ] #4 B: executeCommand() removed; grep confirms no callers in dev tree
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 python evaluate.py --quick reports no new failures
- [ ] #7 Same audit applied to 2.0.0 worktree; mirror task created if code matches
<!-- AC:END -->
