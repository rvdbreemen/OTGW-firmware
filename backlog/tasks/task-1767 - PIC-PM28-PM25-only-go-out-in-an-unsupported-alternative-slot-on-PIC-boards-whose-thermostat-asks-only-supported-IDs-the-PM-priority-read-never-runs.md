---
id: TASK-1767
title: >-
  PIC: PM=28/PM=25 only go out in an unsupported (alternative) slot; on PIC
  boards whose thermostat asks only supported IDs the PM priority read never
  runs
status: To Do
assignee: []
created_date: '2026-11-09T21:51:00Z'
labels:
  - sat
  - pic
  - opentherm
dependencies: []
priority: medium
ordinal: 863000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding from the TASK-1759 / Q-1759a work (draft by otgw-firmware-030509, second reading by otgw-firmware-97, both opened other-projects/otgw-6.6/gateway.asm on 2026-11-09).

In gateway mode with a thermostat the PIC sends exactly one boiler frame per thermostat frame. It replaces a thermostat request only when UnknownMask says the boiler does not support that ID (CreateRequest gateway.asm:2387-2402: 'call UnknownMask', 'goto SendAltRequest'; SendAltRequest :2404-2414 calls Alternative). PM=<id> (SetPrioMessage :4984-4995) only sets prioritymsgid and the PriorityMsg flag; the priority read is sent from Alternative (:3390) -> SendPriorityMsg (:3436), so it uses the same unsupported-slot path (or standalone mode, where MessageTable :1820-1836 picks UserMsg). The EEPROM alternatives list (AlternativeCmd :5479, AA=/DA= :4617-4661) works the same way.

Consequence: on a PIC board whose thermostat asks only IDs the boiler supports (as tjfs's Ideal Halo Lite on OT-Direct, 2026-11-09 capture: IDs 0, 1, 16, 17, 24, 25), PM=28 and PM=25 never go out. This affects:
- entry 276 (SAT asks the PIC for MsgID 25 itself; 'PIC command to be confirmed' - PM=25 only works when such a slot exists),
- the PM=25 flood of entries 45/151 on such installations,
- Q-1759a option C (AA=28) for MsgID 28.
Not measured on a PIC bench with such a thermostat. Related TASK-1759, TASK-1412.5; entries 45, 151, 276, 287. Open questions go to Sergeant D / Robert before any code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The finding is confirmed on a PIC bench or a tester capture (PM=25 / PM=28 with a thermostat that asks only supported IDs: no priority frame on the bus)
- [ ] #2 Robert / Sergeant D decide what SAT does on such PIC installations (recorded as a corrections entry), before any code
- [ ] #3 Entries 45/151/276 and the firmware docs state the limitation
<!-- AC:END -->
