---
id: TASK-1767
title: >-
  PIC: PM=28/PM=25 only go out in an unsupported (alternative) slot; on PIC
  boards whose thermostat asks only supported IDs the PM priority read never
  runs
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T21:51:00Z'
updated_date: '2026-11-16T23:24:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 (d3) Third reading of other-projects/otgw-6.6/gateway.asm, agrees with the finding: in gateway mode CreateRequest (:2387-2402) calls UnknownMask and goes to SendAltRequest (:2404-2414) only for an ID the boiler is known not to support, or when SendUserMessage is set; SendUserMessage is set only by the standalone table (StandAloneMsg :1801 clears it, UserMsg :1837 sets it). Alternative (:3390) reaches SendPriorityMsg (:3436) through InitCmdLoop (:3394-3396), so PM=<id> (SetPrioMessage :4984-4995, sets prioritymsgid + PriorityMsg only) waits for such a slot. Still source-only: AC1 needs a PIC bench with a thermostat that asks only supported IDs, or a tester capture. No code before AC2.

2026-11-15 Coordinator: AC1 cannot be benched (09 has no thermostat on a PIC unit). AC1 is a field AC: a capture from a PIC user whose thermostat asks only boiler-supported IDs, with PM=25 / PM=28 sent; expected no R frame for 25/28. Left In To Do until such a capture exists; AC2 and AC3 follow it.

2026-11-17 ba: AC2 is settled for MsgID 28 by entry 296 (Sergeant D, 2026-11-10, Settled): on a PIC board whose thermostat asks only supported IDs, MsgID 28 does not come and SAT keeps the flow + offset fallback (entries 135, 149, 151); PM=28 every 30 s stays (TASK-1759 part C). For PM=25 (entry 276, the start-up flood of entries 45/151) the same slot limit applies and no entry decides anything different; read as: PM=25 is tried, and without a free slot SAT keeps its fallback (the same reading as 296); not asked separately. AC3: docs/sat/firmware-sat-spec.md now states the limit under the PIC path (gateway.asm 6.6 CreateRequest :2387-2402, SendAltRequest :2404-2414), with tjfs's thermostat IDs and the OT-Direct contrast (ADR-224); the entries themselves are not rewritten (they never are). AC1 (bench or tester capture) stays open: the bench Classic has no thermostat. No code needed.
<!-- SECTION:NOTES:END -->
