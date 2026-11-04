---
id: TASK-737
title: 'fix(otdirect): CS=0 must clear CH enable bit in master status MsgID 0'
status: Done
assignee: []
created_date: '2026-05-27 23:02'
updated_date: '2026-05-29 21:47'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When CS=0 is received, the PIC clears both the MsgID 1 write-override AND bit0 of master status MsgID 0 (CH enable). OTDirect only clears the write-override. This means the boiler continues to receive CH requests in MsgID 0 even after CS=0, which can cause unintended heating. Fix: add otMasterStatusFlags &= ~0x01 after clearWriteOverride(1) in the CS=0 branch of handleOTDirectCommand() (around line 2537).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS=0 handler clears bit0 of otMasterStatusFlags in addition to clearWriteOverride(1)
- [ ] #2 CS=non-zero does NOT clear bit0 (only CS=0 triggers the clear)
- [ ] #3 Build clean, evaluate.py --quick green
- [ ] #4 Manual verification: send CS=0 and confirm MsgID 0 frame no longer has bit0 set
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as duplicate of TASK-731. Both propose the same change (otMasterStatusFlags &= ~0x01 on CS=0). TASK-731 was investigated and closed not-a-bug: gateway.asm SetCtrlSetpoint confirms CS=0 clears only the flow-setpoint override (MsgID 1 write cache) and never touches bit0 (CH enable) of MsgID 0; CH= is the sole owner of that bit. Implementing 737 would regress 731 and remove its defensive comment in OTDirect.ino. No code change.
<!-- SECTION:FINAL_SUMMARY:END -->
