---
id: TASK-1399
title: >-
  Combo image ran OT-Direct on an S3 board WITH a PIC (MAC 10:20:BA:21:B4:F8):
  PIC not detected at boot
status: Done
assignee: []
created_date: '2026-10-25T20:10:00Z'
updated_date: '2026-10-25T21:25:00Z'
labels:
  - combo
  - pic
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 538000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-10-25: the bench unit at 192.168.88.61 (MAC 10:20:BA:21:B4:F8, Robert: an S3 with a PIC) ran alpha.499-505 combo all day with health otcommandinterface 'OT-Direct', device/info otdirectavailable true, otdmode gateway, and SAT commands via handleOTDirectCommand. So the combo boot detection did not see the PIC. Causes to separate: S3 not seated on the carrier / PIC unpowered (memory: 'pic=0 red herring = not on carrier'), or a detection failure (ADR-127/ADR-158/ADR-160, detectPIC retries). If it is a detection bug, a Classic user gets OT-Direct instead of the PIC gateway.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Cause named with evidence: telnet [combo] detect line and PR=A/PIC banner on that board, carrier/power checked
- [ ] #2 If detection bug: fix with old-vs-fix on that board (combo detect line + otcommandinterface PIC)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 (ADR-201 drafting): 'all 3 probes failed every boot' holds only if that unit was in auto (iBoardMode 0); in forced mode 2 it is one probe per boot. Check iBoardMode on that unit first.

2026-10-25 22:45 NOT A BUG. Robert put the S3 on the OTGW32 carrier; USB shows MAC 10:20:BA:21:B4:F8 (COM4), and the unit at .61 reads otdirectavailable true, hardwaremode OT-Direct, I2C 18/17 (OTGW32 pin map), /bootdetect.log boot#1-3 'eMode=2 pic=0 mode=0' (negative verdict not persisted, ADR-201). This MAC is the OTGW32; the premise 'an S3 WITH a PIC ran OT-Direct' came from a wrong board identification earlier that day. OT-Direct was the correct mode.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not a bug: the unit was the OTGW32 (no PIC), so OT-Direct was correct. Evidence: device/info + /bootdetect.log on 10:20:BA:21:B4:F8, 2026-10-25 22:45.
<!-- SECTION:FINAL_SUMMARY:END -->
