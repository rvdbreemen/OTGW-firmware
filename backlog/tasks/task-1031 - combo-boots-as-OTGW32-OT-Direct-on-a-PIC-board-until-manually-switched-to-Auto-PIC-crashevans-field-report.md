---
id: TASK-1031
title: >-
  combo: boots as OTGW32/OT-Direct on a PIC board until manually switched to
  Auto/PIC (crashevans field report)
status: To Do
assignee: []
created_date: '2026-07-09 14:00'
labels: []
dependencies: []
ordinal: 240000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report (Discord #alpha-testing, crashevans, 2026-07-09, msg 1524645517374787594, alpha.337 esp32-combo on S3 Mini Pro + Classic OTGW): after a reboot the device comes up as OTGW32/OT-Direct even though a live PIC is present; only after manually setting board mode back to Auto/PIC does it detect the PIC (and then e.g. PIC flashing works fine). Suspect the TASK-949 first-boot-persist flow: an early verdict (possibly taken while the PIC was slow/absent during a transient) was persisted into iBoardMode=2/OTGW32 and auto-detection never re-runs. Investigate persistence + re-detect policy on combo builds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce or explain the persisted-OTDirect-on-PIC-board state from the TASK-949 detection flow (code-level trace)
- [ ] #2 Fix ensures a PIC board recovers PIC mode automatically (or a clear one-time re-detect path exists) without manual board-mode switching
- [ ] #3 Field validation by an S3 Mini Pro combo tester
<!-- AC:END -->
