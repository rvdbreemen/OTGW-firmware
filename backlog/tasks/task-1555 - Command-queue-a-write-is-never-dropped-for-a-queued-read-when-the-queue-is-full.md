---
id: TASK-1555
title: >-
  Command queue: a write is never dropped for a queued read when the queue is
  full
status: To Do
assignee: []
created_date: '2026-11-01T22:29:00Z'
updated_date: '2026-11-02T04:37:00Z'
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
- [ ] #1 Host test: a full queue of reads plus one write keeps the write (a queued read is evicted instead); red on the current code
- [ ] #2 No change for a queue that is not full; bench .63 shows writes still echoed by the PIC
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: OTGW-Core.ino:3866 returns when the queue is full, a write is dropped silently, no read is evicted. TASK-1315 landed. Agent-doable, host red/green, bench on .63.
<!-- SECTION:NOTES:END -->
