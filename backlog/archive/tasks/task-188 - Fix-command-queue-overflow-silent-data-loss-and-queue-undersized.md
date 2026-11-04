---
id: TASK-188
title: 'Fix: command queue overflow - silent data loss and queue undersized'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:37'
updated_date: '2026-04-08 22:53'
labels:
  - audit
  - otgw32
  - phase-6
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 6 audit (TASK-163) found two problems with the OTDirect command queue:\n1. 8 direct otCmdEnqueue() callers ignore the return value and silently drop frames on queue full. Only enqueueWriteCommand() logs the drop.\n2. Queue has 7 usable slots (size 8, 1 wasted). The MH+MW+M2+CS+MM+SW+SH+HW=P burst scenario produces 8 frames and overflows by one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add return-value checks to all 8 direct otCmdEnqueue() callers (HW=P, MH=, MW=, M2=, RR=, PM=, RS=, TP=)
- [x] #2 Add DebugTf warning log on queue-full drop for each caller
- [x] #3 Review and increase queue size from 8 to 12 to handle burst scenarios
- [x] #4 Verify no frame is silently lost in the MH+MW+M2+CS+MM+SW+SH+HW=P burst test
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed silent data loss on queue full and increased queue size from 8 to 12.

Changes:
- OT_CMD_QUEUE_SIZE increased from 8 to 12 to handle command bursts
- Added if (!otCmdEnqueue(...)) { DebugTf(PSTR("OTD: queue full, dropped frame %08lX
"), frame); } guard to all 8 direct callers that previously ignored the return value:
  - DHW push MsgID 99 frame (HW=P path)
  - MH= MsgID 99 frame
  - MW= MsgID 99 frame
  - M2= MsgID 99 frame
  - RR= MsgID 4 frame
  - PM= one-shot READ frame
  - RS= counter reset WRITE frame
  - TP= TSP frame
- enqueueWriteCommand() helper already had a drop log, left unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
