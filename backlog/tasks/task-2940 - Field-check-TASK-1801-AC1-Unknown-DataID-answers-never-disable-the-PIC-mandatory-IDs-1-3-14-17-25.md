---
id: TASK-2940
title: >-
  Field check TASK-1801 AC1: Unknown-DataID answers never disable the
  PIC-mandatory IDs 1/3/14/17/25
status: To Do
assignee: []
created_date: '2026-11-20T01:25:00Z'
labels:
  - otdirect
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1071000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1801 AC1 (OT-Direct, host-proven by tests/test_otd_mandatory_ids.py 5/5, red 3 on the old code; shipped beta.293). Needs a boiler that answers Unknown-DataID to 1/3/14/17/25. Waived for 2.0.0 by Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529): close the 2.0.0 task with a field waiver and one 2.1.0 follow-up per field AC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Unknown-DataID answers to 1, 3, 14, 17 and 25 never disable these IDs or turn them into alternative slots, as the PIC's MandatoryID (field capture on OT-Direct with a boiler that answers Unknown-DataID to one of them)
<!-- AC:END -->
