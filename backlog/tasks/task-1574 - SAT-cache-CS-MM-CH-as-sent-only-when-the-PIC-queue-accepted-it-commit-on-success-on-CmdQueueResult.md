---
id: TASK-1574
title: >-
  SAT: cache CS/MM/CH as sent only when the PIC queue accepted it
  (commit-on-success on CmdQueueResult)
status: To Do
assignee: []
created_date: '2026-11-02T21:46:00Z'
labels:
  - sat
dependencies:
  - TASK-1570
priority: high
ordinal: 718000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Peer review finding (queue lane, 2026-11-02) plus TASK-1570 advisor point 1: satSendCS/satEnqueueIfChangedCS/satEnqueueOwnCS/satSendMM/satSendCH update fLastSentCS/iLastSent*Ms and the on-change flags even when addCommandToQueue dropped the command (full queue of writes, lock timeout). A dropped CS=0 release is never resent (the PIC keepalive skips values below 8), and since TASK-1570 a dropped MM=/CH= waits for the 300 s repair. TASK-1569 makes addCommandToQueue return CmdQueueResult {Queued, Replaced, Sent, Dropped}; cache only when != Dropped.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: with addCommandToQueue returning Dropped, the CS/MM/CH cache and the on-change flags stay unchanged and the next tick or resend sends again; with Queued/Replaced the behaviour is as today (red before, green after)
- [ ] #2 A dropped CS=0 hand-back is retried until accepted
- [ ] #3 Full tests folder green, evaluate --quick 0 failed, bench .63 health
<!-- AC:END -->
