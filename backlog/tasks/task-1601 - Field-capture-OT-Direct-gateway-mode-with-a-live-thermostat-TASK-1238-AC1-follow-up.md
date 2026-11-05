---
id: TASK-1601
title: >-
  Field capture: OT-Direct gateway mode with a live thermostat (TASK-1238 AC1
  follow-up)
status: To Do
assignee: []
created_date: '2026-11-04T05:49:00Z'
updated_date: '2026-11-05T18:09:00Z'
labels:
  - otdirect
  - field
  - needs-field-data
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 742000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1238 AC1 was waived by Robert 2026-11-04: capture from an OT-Direct user with a thermostat in gateway mode, to see which CH-enable bit the boiler acts on and validate ADR-208.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture transcript with thermostat MsgID 0 frames and the boiler reaction, attached to the task
- [ ] #2 ADR-208 consequences confirmed or a revisit task filed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-05 (session otgw-firmware-54, AskUserQuestion): field request, wait. Label needs-field-data marks the wait (TASK-1647 convention). The coordinator posts one request in #alpha-testing for an OT-Direct user with a thermostat in gateway mode; Done when a capture is attached.

2026-11-05 19:15: field request posted in #alpha-testing (lead session) asking an OT-Direct gateway-mode user for a ~30 min capture with a heating demand.
<!-- SECTION:NOTES:END -->
