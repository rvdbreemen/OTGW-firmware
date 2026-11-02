---
id: TASK-1555
title: >-
  Command queue: a write is never dropped for a queued read when the queue is
  full
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T22:29:00Z'
updated_date: '2026-11-02T06:39:00Z'
labels:
  - firmware
  - command-queue
dependencies: []
priority: high
ordinal: 708000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1393 (lead triage 2026-11-01). addCommandToQueue() drops a new command when CMDQUEUE_MAX (20) is reached ('Reached max queue', OTGW-Core.ino ~3866), so a burst of periodic reads can make a write (HW=, CS=, TT=, ...) disappear. Idea from maarten_41762 (he asked not to act on it then). Build only after TASK-1315 (beta.3) landed, it changes the same function.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a full queue of reads plus one write keeps the write (a queued read is evicted instead); red on the current code
- [ ] #2 No change for a queue that is not full; bench .63 shows writes still echoed by the PIC
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reads = PR=, PM= and TP= without ':' (TP=index reads a transparent slave parameter; TP=index:value writes it); everything else is a write. In addCommandToQueue() (OTGW-Core.ino), at the 'Reached max queue' check: if the new command is a write, evict a queued read and append the write; the victim is the newest read with retrycnt 0 (never sent, so no PIC reply can be mismatched), else the newest read; no read in the queue = drop as today; a read into a full queue is dropped as today. Eviction logged on OT debug and as a WebSocket '!' event '<cmd> [evicted for a write]'; not published as command_dropped (that topic means 'unanswered after 5 tries' and latches command_retry_exceeded). SAT's PM chain: an evicted chain PM= times out after PM_CHAIN_TIMEOUT_MS (60 s) and is re-asked once, so the chain does not stall. Helpers isQueueReadCmd()/findEvictableRead() between removeFromCmdQueue() and addCommandToQueue() (inside the harness slice). Verification: AC1 test/host/test_cmd_queue_drop.cpp Q1-Q3 (defect, red on HEAD 233e11ade: FAIL x3) + Q4-Q6 controls, --old-rev compare; AC2 bench .63: a burst of PM= that fills the queue plus one write (e.g. PR= reads + TT=), check the write is echoed by the PIC, and normal writes still echoed. 1.x: not ported (1.x queue differs; no field report).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: OTGW-Core.ino:3866 returns when the queue is full, a write is dropped silently, no read is evicted. TASK-1315 landed. Agent-doable, host red/green, bench on .63.

Review round (opus, 2026-11-02): HIGH WebSocket event + cMsg written from async_tcp (single-writer contract TASK-1321) -> removed, OT debug line only; MEDIUM queue shift from async_tcp without lock -> the write now takes the evicted read's slot in place (no shift, no size change, same as the same-prefix replace path); LOW no mixed read/write case -> Q7 added. TP= classification corrected after advisor check: syntax TP=<id>:<entry>[=<value>] (OTGW-Commands.md:788), a write has a second '='. Evidence AC1: python test/host/test_cmd_queue_drop.py --old-rev origin/dev -> RESULT: PASS (FIX all cases; OLD fails Q1,Q2,Q3,Q7; OLD passes controls D1-D4,C1,C2,R1-R4,Q4-Q6). Full tests/test_*.py: no failure; evaluate --quick 0 failed, 100%. Bench old (beta.7, SAT sim off for PIC echoes): 25 PM= via REST then PS=0 -> 'Reached max queue' for PS=0 (07:10:01.038), never sent; a PS=0 into a non-full queue was sent and echoed 'PS: 0'.
<!-- SECTION:NOTES:END -->
