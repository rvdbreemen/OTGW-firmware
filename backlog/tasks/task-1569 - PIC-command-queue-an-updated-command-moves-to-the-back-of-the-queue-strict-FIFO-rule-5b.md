---
id: TASK-1569
title: >-
  PIC command queue: an updated command moves to the back of the queue (strict
  FIFO, rule 5b)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T20:44:00Z'
updated_date: '2026-11-02T22:15:00Z'
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
- [x] #2 An updated command moves to the back: host test, queue [MM=80, CS=45] then MM=90 gives send order CS=45, MM=90 (old code: MM=90, CS=45)
- [x] #3 Immergas: TP=11:12=x offered before MM=x reaches the PIC before MM= (host test with the send-time harness)
- [ ] #4 Existing queue tests (tests/test_pic_pm_chain.py, TASK-1555 eviction) green; bench .63 with SAT on, no reboot, settings kept
- [x] #5 TP=11:12=x then MM=y, both already queued, updated in that order: TP= is sent before MM= (settles Q-1537b ordering; host test, old code MM first)
- [x] #6 addCommandToQueue returns a result (queued / replaced / dropped) so a caller (SAT, TASK-1570) can see a dropped enqueue; existing callers compile unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 independent review (opus, read-only) of b20ffc6f4: no deadlock, every queue access locked. Fixed on lane-q3: (1) pmChainStart took the lock unchecked and held it on the OT-Direct path, where handleOTDirectCommand -> processOT takes OTStateLock (wrong order): lock now only on the PIC path, checked; a dropped chain PM= counts as missed after 5 s instead of 60 s; handleCommandQueue returns at once without a PIC. (3) rule 5b was by due time, not offering order: the drain now sends the first due entry in slot order and moves a sent, unconfirmed entry to the back (strict FIFO, retries queue behind newer commands, 20-deep CS still 19 s / 950 ms with echoes). (4) PR=A banner: lock scope ends before reportOTGWEvent. (5) a late echo of a replaced command no longer removes the replacement (test_stale_echo...: red before, green after). Handed to the lead (SATcontrol.ino, TASK-1570): (2) satEnqueueOwnCS ignores Dropped and caches the CS as sent; (6) pre-existing: _satCmdInFlight is read in addCommandToQueue before the lock.

2026-11-02 handover to the lead (lane-q2 ../wt-q2, no bump, series 537fd5a63..HEAD, one commit per task). Host: tests/test_pic_pm_chain.py 28/28 at the top; each task's new tests red on the code before it (see commit/test names), green at its own commit; evaluate --quick 0 failed. Open: bench on .63 after the 08:00 release (lead tags, builds, benches each task), firmware compile check of the top running at handover.
<!-- SECTION:NOTES:END -->
