---
id: TASK-731
title: 'fix(otdirect): CS=0 must clear CH enable bit in master status MsgID 0'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 18:58'
updated_date: '2026-05-27 19:58'
labels: []
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
When CS=0 is received, the PIC clears both the MsgID 1 write-override AND bit0 of master status MsgID 0 (CH enable). OTDirect only clears the write-override (handleOTDirectCommand ~line 2537). This means the boiler continues to receive CH requests in MsgID 0 even after CS=0, which can cause unintended heating.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CS=0 handler adds otMasterStatusFlags &= ~0x01 after clearWriteOverride(1)
- [ ] #2 CS=non-zero does NOT clear bit0 (only CS=0 triggers the clear)
- [ ] #3 Build clean, evaluate.py --quick green
- [ ] #4 Manual test: send CS=0 and confirm MsgID 0 no longer has CH-enable bit set
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gesloten als not-a-bug. CS=0 clearet uitsluitend de flow setpoint override (MsgID 1 write cache). Het raakt bit0 (CH enable) van MsgID 0 NIET aan. gateway.asm SetCtrlSetpoint bevestigt dit: CS=0 zet alleen de SysCtrlSetpoint vlag en wijzigt nooit bit0. CH= is de enige eigenaar van dat bit. Een defensieve comment is toegevoegd aan OTDirect.ino om toekomstige her-analyse te voorkomen.
<!-- SECTION:FINAL_SUMMARY:END -->
