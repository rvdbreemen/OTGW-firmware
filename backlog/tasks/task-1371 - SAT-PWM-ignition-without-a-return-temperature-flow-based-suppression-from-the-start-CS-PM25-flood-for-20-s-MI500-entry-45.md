---
id: TASK-1371
title: >-
  SAT PWM ignition without a return temperature: flow-based suppression from the
  start, CS/PM=25 flood for 20 s, MI=500 (entry 45)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T22:32:00Z'
updated_date: '2026-10-30T14:48:00Z'
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Basis: plan posted 2026-10-30 09:26 (msg 1565658169429196822) as amended by entries 149 + 151. Scope ONLY boilers without a return temperature: MsgID 28 answered Unknown-DataID AND no valid 'R' board sensor (a reading counts for 5 min; entries 84, 135). All other boilers keep thermo-nova's hold unchanged (heating_control.py:397-448).
1. Start-up hold (entries 149, 151): at the start of the PWM ON phase and at every CH flame off->on (also the relight after a hot-water draw, entry 142), for 20 s: alternate CS = flow + flame-off offset (default 18), clamped to [cold setpoint, effective max setpoint incl. TASK-1373 user MaxTSet] (as heating_control.py:397-405 does for return + offset), at t = 0, 2, 4 ..., and PM=25 at t = 1, 3, 5 .... After 20 s: suppression CS = flow - 1 (heating_control.py:439-448). Before the flame lights the hold value applies (no flow - 1 below the boiler water).
2. PM=25 goes through the PIC command queue without wiping the boot PM=3/15/48 chain (pmChain); OT-Direct: the equivalent read request of MsgID 25.
3. Coupling TASK-1370 (ADR-203): the flood's CS goes through satSendCS() so the 5 s resend repeats the flood value, or the resend pauses during the 20 s; no stale tick CS may interleave (PIC queue replaces a pending same-prefix CS=, OTGW-Core.ino:3809-3818). The 3 s PWM ON refresh must not overwrite the flood either.
4. MI=500 (entry 141): once on PIC boards after the no-return detection (and for the Ideal/Intergas/Nefit quirk as today), never on OT-Direct, no reset. Today MI=500 also goes out at boot on OT-Direct (SATcontrol.ino boot block 'Manufacturer boot quirk'): gate it to PIC.
Files: SATcontrol.ino (PWM ON / flame edge / hold), SATtypes.h (state), tests. Verification: host test of the command sequence (CS/PM=25 alternation for 20 s with flow + 18 clamped, then flow - 1; relight after DHW; normal boiler unchanged; MI=500 PIC only, not OT-Direct; resend does not interleave) red on HEAD 5e8366d3f, green on fix; bench on .63 in simulation if the sim can model a missing MsgID 28 (else host evidence + note). Cross-line: dev only.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 implement-next-task: skipped, waits for Q40 (how 'no return temperature' is detected) and Sergeant D's OK on the plan.

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-28: Q40 B 'for now, we may need to reconsider later' (entry 84): no return temperature = Unknown-DataID on MsgID 28, provisional.

TASK-1412.1 records review (R28): 2026-10-29. record reconciliation (TASK-1412.1, R28; authored clarification, not a Sergeant D decision). Status of entry 45: in force, not superseded. Refined provisionally by entry 84 (Q40 B "for now": no return temperature = Unknown-DataID on MsgID 28 via isBoilerMsgIdUnsupportedRead(28)). Its precondition, that the ON phase lasts through ignition, is met by entry 80 / TASK-1382 (completed); entry 80 line 3615 notes the Q40 B interaction was not checked. Evidence class: a recorded design decision that deliberately deviates from thermo-nova heating_control.py:392-395. No measurement supports the light-off premise (the flood prevents a stalled ignition), and none supports MI=500 having any effect. Treat the command sequence as experimental (remediation plan line 59): a host test of the CS/PM=25 sequence is software evidence only, and physical light-off acceptance belongs to TASK-1412.12 AC#3 / LL-018. Entry 119 (alpha.527 field run, 'works as intended') is not evidence for entry 45: the flood is not implemented (SATcontrol.ino:978-981) and gruijter's Intergas has a return value through the board sensor (entries 6, 84 Open #2). Current code (entry line numbers are from their own snapshot): satPwmFlameOffHold() SATcontrol.ino:835-841 uses raw Tret (satGetReturnTemp() :1776-1780, no validity check), floored at the cold setpoint; MI=500 only as a manufacturer boot quirk at :4342-4346 (Ideal/Intergas/Nefit, :202/:204/:206). Not re-verified at HEAD: audit #45's finding that it effectively never fires. Scope, our reading: entry 95's Settled bullet adds 'without a return temperature, the entry 45 flood' to the post-draw relight. His words were 'perform the start-up sequence to lit the fire', so the post-draw path (SATcontrol.ino:977-981) is in scope only as our reading. MI in gateway mode, our reading of the PIC source only: otgw-6.6 gateway.asm MsgInterval only paces stand-alone messages (MsgTrigger :1561, :1567-1572, consumed at :1793-1815, all paths gated on NoThermostat :1751-1756, :1770-1771, :1791-1792), so MI=500 appears to have no effect with an OpenTherm thermostat attached. Confirm with Schelte (PIC 6.8) before the plan relies on it. Still open: entry 45 Open #3 (OT-Direct equivalent, our design choice, to be shown in the plan); entry 45 Open #4 (flood also when a return temperature exists); entry 84 Open #2 (board sensor 'R' vs Unknown-DataID, 'Re-ask him', now queued as Q61, not asked yet); entry 84 Open #3 (Data-Invalid/stale, his 'reconsider later'). TASK-1412.12 AC#2 also needs the sample-age comparison with the application spec; this note does not cover that. Not queued as questions: whether the flood also runs for boilers that report a return temperature (entry 45 Open #4) and the post-draw flood scope (entry 95). If asked, they are two separate questions.

TASK-1412.1 records review (R35): 2026-10-29. R35 audit (TASK-1412.1): entry 84 settles 'no return temperature' = Unknown-DataID on MsgID 28 only, provisionally. Still open before this task's plan goes to Sergeant D: (a) entry 84 Open #2, board sensor 'R' Tret vs an Unknown-DataID verdict on 28 (queued as Q61 in docs/sat/open-questions.md, not asked yet); (b) entry 84 Open #3, Data-Invalid/stale Tret, his "reconsider later", tracked under TASK-1412.5; (c) entry 84 Open #1, whether a verdict is ever formed (OT-Direct 10 s poll, OTDirect.ino:147; boilerEvidence not traced). At f4b8be6d9 SAT reads raw Tret (SATcontrol.ino:1776-1780) and does not call isBoilerMsgIdUnsupportedRead (OTGW-Core.ino:5112-5113).

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Robert 2026-10-30: a valid board 'R' sensor reading counts as a return temperature for at most 5 minutes (5 missed PR=E polls) before the boiler counts as without return temperature (entry 135 open point / TASK-1412.5). Plan drafted: satNoReturnTemp() = isBoilerMsgIdUnsupportedRead(28) && !picBoardTretValid(); flood helper on the CH flame-on edge; MI=500 on PIC only after detection, initSAT() send removed (live entry 141 bug on OT-Direct); open question to Sergeant D: CS before the flame lights.

2026-10-30 11:30 UTC: Sergeant D answered the follow-up with A (msg 1565666884467298335), recorded as entry 151: the CS/PM=25 flood runs inside the 20 s hold with CS = flow + 18, then suppression flow - 1. Plan cleared to build.

2026-10-30 coordinator (session 80): plan inputs before implementation. (1) Entries 149+151 replace the title's 'flow-based suppression from the start': first 20 s after each CH flame off->on (incl. relight after a draw, entry 142) alternate CS = flow + 18 (t=0,2,4..) and PM=25 (t=1,3,5..), then suppression CS = flow - 1. (2) Coupling with TASK-1370/ADR-203: satCmdResend re-sends the cached fLastSentCS every 5 s; the flood must send CS through satSendCS() (updating the cache) or pause the resend during the flood, else a stale tick CS interleaves (on PIC boards addCommandToQueue replaces a pending same-prefix CS=, OTGW-Core.ino:3809-3818). (3) Coupling with TASK-1373: flow + 18 is clamped to the effective max setpoint (entry 151 clamp to [cold, max]), i.e. satEffectiveMaxSetpoint incl. the user MaxTSet. Implement on top of the integrated 1370+1373 code.

Plan correction (self-review against heating_control.py:392-433): timing follows thermo-nova's hold, with flow in place of return. (a) PWM ON, flame not yet lit: hold CS = flow + flame-off offset (18), clamped to [cold, effective max], refreshed as the normal PWM ON refresh (as :397-407 with return). (b) From each CH flame off->on (incl. relight after a draw, entry 142), for the suppression delay (20 s, :414-433): the flood: CS = the same held value at t = 0, 2, 4 ... and PM=25 at t = 1, 3, 5 ... (entry 151 A). (c) After 20 s from flame-on: suppression CS = flow - 1 (:439-448). The flood therefore starts at flame-on, not at the PWM ON start.

2026-10-30 implementation (sonnet lane in wt-1371, uncommitted): satNoReturnTemp() = isBoilerMsgIdUnsupportedRead(28) && !picBoardTretValid() (new, 5 min after a valid PR=E with sensor R); hold uses flow + offset for such boilers; flood steps (CS even s via satSendCS, PM=25 odd s, skipped while pmChainBusy()) for fModSupDelay after each CH flame-on; MI=500 moved to satMiStep() after satRestartStep, PIC only. Host test tests/test_sat_no_return_ignition.py: base Ran 15 FAILED (failures=10), fix 15 OK. Interpretations recorded (not silent): (1) flood CS recomputed live = flow + 18 each even second, read from entry 45 ('flow gives responsiveness', 'frequent CS updates'); thermo-nova freezes its hold value (heating_control.py:418-423) - to confirm with Sergeant D after Q-b; (2) flood length follows the suppression-delay setting (default 20 s); (3) flood only in PWM ON with overshoot protection, same gates as the 3 s refresh. Detection premise (entry 84 open points 1/3) unconfirmed; no simulation of a missing MsgID 28 exists, so bench evidence is limited to normal-boiler non-regression. Reviewers (opus correctness, sonnet SAT-entries) running.
<!-- SECTION:NOTES:END -->
