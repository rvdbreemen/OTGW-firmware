---
id: TASK-1371
title: >-
  SAT PWM ignition without a return temperature: flow-based suppression from the
  start, CS/PM=25 flood for 20 s, MI=500 (entry 45)
status: To Do
assignee: []
created_date: '2026-10-24T22:32:00Z'
updated_date: '2026-10-28T06:48:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.5
  - TASK-1412.11
  - TASK-1382
references:
  - TASK-1412
priority: high
ordinal: 506000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 21:34 UTC (Q14, entry 45, a deliberate deviation from thermo-nova heating_control.py:392-395): when the boiler reports no return temperature, skip the flame-off hold and suppress to flow - offset from the start of the PWM ON phase; during the first 20 s of ignition alternate CS (t=0,2,4..) and PM=25 (t=1,3,5..) one command per second; send MI=500 to the PIC for such boilers. Open points in entry 45 (detection of 'no return temperature', MI in gateway mode, OT-Direct equivalent). Plan goes to Sergeant D first. Fixes audit #27; relates to audit #45 (MI=500 never sent) and #53 (no PM=25).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Without a return temperature the PWM ON phase suppresses to flow - offset from ignition and sends CS/PM=25 alternately each second for 20 s (host test of the command sequence red on HEAD, green); MI=500 sent for such boilers on PIC boards; plan OK'd by Sergeant D first
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 implement-next-task: skipped, waits for Q40 (how 'no return temperature' is detected) and Sergeant D's OK on the plan.

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q40 B 'for now, we may need to reconsider later' (entry 84): no return temperature = Unknown-DataID on MsgID 28, provisional.
<!-- SECTION:NOTES:END -->
