---
id: TASK-1569
title: >-
  PIC command queue: an updated command moves to the back of the queue (strict
  FIFO, rule 5b)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T20:44:00Z'
updated_date: '2026-11-02T21:21:00Z'
labels:
  - pic
  - command-queue
milestone: 2.0.0
dependencies:
  - TASK-1567
priority: high
ordinal: 718000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02, queue design rule 5: choose b. Today addCommandToQueue() dedups a same-prefix command in place: the value is updated but the slot keeps its place, so a later-offered command can go out after an earlier-updated one (e.g. Immergas TP=11:12 before MM=, entry 233: MM= keeps its older slot and goes first). With b, the order of offering is the order of sending. Prerequisite found in the code: addCommandToQueue() also runs on the async_tcp task (REST) while handleCommandQueue() walks the queue on the loop task; moving a slot shifts entries, which the TASK-1555 eviction deliberately avoids. So this task first adds one lock around every queue mutation and walk (addCommandToQueue, handleCommandQueue, checkCommandResponse, removeFromCmdQueue, pmChain*), then moves an updated entry to the back. Depends on TASK-1567 (drip).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 One lock guards every queue read/write path; a host or on-target stress test (REST adds from a second task while the loop drains) shows no lost or duplicated entry
- [ ] #2 An updated command moves to the back: host test, queue [MM=80, CS=45] then MM=90 gives send order CS=45, MM=90 (old code: MM=90, CS=45)
- [ ] #3 Immergas: TP=11:12=x offered before MM=x reaches the PIC before MM= (host test with the send-time harness)
- [ ] #4 Existing queue tests (tests/test_pic_pm_chain.py, TASK-1555 eviction) green; bench .63 with SAT on, no reboot, settings kept
- [ ] #5 TP=11:12=x then MM=y, both already queued, updated in that order: TP= is sent before MM= (settles Q-1537b ordering; host test, old code MM first)
- [ ] #6 addCommandToQueue returns a result (queued / replaced / dropped) so a caller (SAT, TASK-1570) can see a dropped enqueue; existing callers compile unchanged
<!-- AC:END -->
