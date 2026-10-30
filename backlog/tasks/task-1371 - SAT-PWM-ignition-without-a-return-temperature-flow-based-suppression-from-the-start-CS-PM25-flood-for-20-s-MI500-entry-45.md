---
id: TASK-1371
title: >-
  SAT PWM ignition without a return temperature: flow-based suppression from the
  start, CS/PM=25 flood for 20 s, MI=500 (entry 45)
status: To Do
assignee: []
created_date: '2026-10-24T22:32:00Z'
updated_date: '2026-10-30T06:58:00Z'
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

TASK-1412.1 records review (R28): 2026-10-29. record reconciliation (TASK-1412.1, R28; authored clarification, not a Sergeant D decision). Status of entry 45: in force, not superseded. Refined provisionally by entry 84 (Q40 B "for now": no return temperature = Unknown-DataID on MsgID 28 via isBoilerMsgIdUnsupportedRead(28)). Its precondition, that the ON phase lasts through ignition, is met by entry 80 / TASK-1382 (completed); entry 80 line 3615 notes the Q40 B interaction was not checked. Evidence class: a recorded design decision that deliberately deviates from thermo-nova heating_control.py:392-395. No measurement supports the light-off premise (the flood prevents a stalled ignition), and none supports MI=500 having any effect. Treat the command sequence as experimental (remediation plan line 59): a host test of the CS/PM=25 sequence is software evidence only, and physical light-off acceptance belongs to TASK-1412.12 AC#3 / LL-018. Entry 119 (alpha.527 field run, 'works as intended') is not evidence for entry 45: the flood is not implemented (SATcontrol.ino:978-981) and gruijter's Intergas has a return value through the board sensor (entries 6, 84 Open #2). Current code (entry line numbers are from their own snapshot): satPwmFlameOffHold() SATcontrol.ino:835-841 uses raw Tret (satGetReturnTemp() :1776-1780, no validity check), floored at the cold setpoint; MI=500 only as a manufacturer boot quirk at :4342-4346 (Ideal/Intergas/Nefit, :202/:204/:206). Not re-verified at HEAD: audit #45's finding that it effectively never fires. Scope, our reading: entry 95's Settled bullet adds 'without a return temperature, the entry 45 flood' to the post-draw relight. His words were 'perform the start-up sequence to lit the fire', so the post-draw path (SATcontrol.ino:977-981) is in scope only as our reading. MI in gateway mode, our reading of the PIC source only: otgw-6.6 gateway.asm MsgInterval only paces stand-alone messages (MsgTrigger :1561, :1567-1572, consumed at :1793-1815, all paths gated on NoThermostat :1751-1756, :1770-1771, :1791-1792), so MI=500 appears to have no effect with an OpenTherm thermostat attached. Confirm with Schelte (PIC 6.8) before the plan relies on it. Still open: entry 45 Open #3 (OT-Direct equivalent, our design choice, to be shown in the plan); entry 45 Open #4 (flood also when a return temperature exists); entry 84 Open #2 (board sensor 'R' vs Unknown-DataID, 'Re-ask him', now queued as Q61, not asked yet); entry 84 Open #3 (Data-Invalid/stale, his 'reconsider later'). TASK-1412.12 AC#2 also needs the sample-age comparison with the application spec; this note does not cover that. Not queued as questions: whether the flood also runs for boilers that report a return temperature (entry 45 Open #4) and the post-draw flood scope (entry 95). If asked, they are two separate questions.

TASK-1412.1 records review (R35): 2026-10-29. R35 audit (TASK-1412.1): entry 84 settles 'no return temperature' = Unknown-DataID on MsgID 28 only, provisionally. Still open before this task's plan goes to Sergeant D: (a) entry 84 Open #2, board sensor 'R' Tret vs an Unknown-DataID verdict on 28 (queued as Q61 in docs/sat/open-questions.md, not asked yet); (b) entry 84 Open #3, Data-Invalid/stale Tret, his "reconsider later", tracked under TASK-1412.5; (c) entry 84 Open #1, whether a verdict is ever formed (OT-Direct 10 s poll, OTDirect.ino:147; boilerEvidence not traced). At f4b8be6d9 SAT reads raw Tret (SATcontrol.ino:1776-1780) and does not call isBoilerMsgIdUnsupportedRead (OTGW-Core.ino:5112-5113).

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.
<!-- SECTION:NOTES:END -->
