---
id: TASK-1752
title: 'OT-Direct: flush queued boiler writes on a mode switch'
status: To Do
assignee: []
created_date: '2026-11-09T09:30:00Z'
labels:
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 854000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). resetTransientState() (OTDirect.ino ~2445-2510) clears tables, caches and state on a mode switch but not otCmdQueue, so a WRITE 16/100 queued for a TT= in master mode can still go out once after switching to gateway mode (through the scheduler or an alternative slot). Low impact (one frame), but it contradicts ADR-223's 'no gateway WRITE 16/100 in gateway mode'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a mode switch no frame queued under the previous mode reaches the boiler; host test red before, green after
<!-- AC:END -->
