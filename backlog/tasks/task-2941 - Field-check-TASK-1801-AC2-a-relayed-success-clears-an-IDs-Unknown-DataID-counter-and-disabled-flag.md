---
id: TASK-2941
title: >-
  Field check TASK-1801 AC2: a relayed success clears an ID's Unknown-DataID
  counter and disabled flag
status: To Do
assignee: []
created_date: '2026-11-20T01:26:00Z'
labels:
  - otdirect
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1072000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1801 AC2 (OT-Direct, host-proven by tests/test_otd_mandatory_ids.py). Needs a boiler that answers Unknown-DataID to a non-mandatory ID and later succeeds. Waived for 2.0.0 by Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529): close the 2.0.0 task with a field waiver and one 2.1.0 follow-up per field AC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A relayed success of an ID clears both its counter and its disabled flag (field capture on OT-Direct)
<!-- AC:END -->
