---
id: TASK-1399
title: >-
  Combo image ran OT-Direct on an S3 board WITH a PIC (MAC 10:20:BA:21:B4:F8):
  PIC not detected at boot
status: To Do
assignee: []
created_date: '2026-10-25T20:10:00Z'
updated_date: '2026-10-25T20:32:00Z'
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
<!-- SECTION:NOTES:END -->
