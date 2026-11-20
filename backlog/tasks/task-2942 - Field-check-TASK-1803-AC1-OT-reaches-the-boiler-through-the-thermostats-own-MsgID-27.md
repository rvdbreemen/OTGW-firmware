---
id: TASK-2942
title: >-
  Field check TASK-1803 AC1: OT= reaches the boiler through the thermostat's own
  MsgID 27
status: To Do
assignee: []
created_date: '2026-11-20T01:27:00Z'
labels:
  - otdirect
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1073000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1803 AC1 (OT-Direct gateway mode, host-proven by tests/test_otd_relay_rw.py; .61 has no thermostat; tjfs's beta.161 captures have no OT= command). Needs a thermostat in OT-Direct gateway mode. Waived for 2.0.0 by Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529): close the 2.0.0 task with a field waiver and one 2.1.0 follow-up per field AC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With a thermostat in gateway mode, OT= reaches the boiler through the thermostat's own MsgID 27 and the thermostat reads the OT= value (field capture)
<!-- AC:END -->
