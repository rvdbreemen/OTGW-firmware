---
id: TASK-2943
title: >-
  Field check TASK-1803 AC2: SW=/SH= turn the thermostat's next READ 56/57 into
  one WRITE
status: To Do
assignee: []
created_date: '2026-11-20T01:28:00Z'
labels:
  - otdirect
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1074000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1803 AC2 (OT-Direct gateway mode, host-proven by tests/test_otd_relay_rw.py). Needs a thermostat in OT-Direct gateway mode. Waived for 2.0.0 by Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529): close the 2.0.0 task with a field waiver and one 2.1.0 follow-up per field AC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SW= and SH= turn the thermostat's next READ 56/57 into one WRITE, as the PIC (including its MsgID 6 permission gate) (field capture)
<!-- AC:END -->
