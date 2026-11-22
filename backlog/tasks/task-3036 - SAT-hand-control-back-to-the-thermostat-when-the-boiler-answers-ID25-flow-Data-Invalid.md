---
id: TASK-3036
title: >-
  SAT: hand control back to the thermostat when the boiler answers ID25 (flow)
  Data-Invalid
status: To Do
assignee: []
created_date: '2026-11-22T16:43:00Z'
updated_date: '2026-11-22T16:56:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1144000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: entry 471 in the private SAT register (Sergeant D, Q-470a answer 1: an ID25 Data-Invalid stops SAT and hands back to the thermostat; 'a flow-sensor fault stops the boiler anyway'; #dev-sat-mqtt message 1574102255043477646); amends entry 470. Cite the entry by number. No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: red with a Data-Invalid ID25 reply on the old code, green on the fix
- [ ] #2 Bench on .63 with the SAT simulator
- [x] #3 Blocked until Sergeant D answers Q-471a: entry 471 replaces entries 218/241 (today flow Data-Invalid / 60 s stale = flow missing: PWM idle, continuous setpoint, SAT keeps control; SATcontrol.ino:3045-3052, :3105-3109) and the 60 s timeout is confirmed; the answer is recorded as an entry before coding
- [ ] #4 An ID25 (flow) Data-Invalid answer, or a flow stale for more than 60 s (entry 241's limit), stops SAT and hands control back to the thermostat (entry 472), with a narrate line and an MQTT status; this replaces the keep-control flow-missing behaviour of entries 218/241 (SATcontrol.ino:3045-3052, :3105-3109)
- [ ] #5 When a valid flow returns, SAT re-enters through its normal start path (our interpretation, marked as such in the code comment and the ADR; Sergeant D did not decide it)
- [ ] #6 Entries 471 and 472 are cited in the code comment, the task and the commit; an ADR via adr-kit that supersedes the flow-missing part tied to entries 218/241
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22 (coordinator): BLOCKED on Q-471a. Entry 471 conflicts with entries 218/241: today a flow Data-Invalid or a flow stale for 60 s counts as 'flow missing': PWM idle, continuous setpoint, SAT keeps control (SATcontrol.ino:3045-3052, satGetFlowTemp :3105-3109). So this task changes the flow-missing behaviour; it is not a new rule. Sergeant D must confirm that 471 replaces 218/241, and the 60 s timeout, before any code.

2026-11-22 UNBLOCKED: Sergeant D answered Q-471a 'A' (#dev-sat-mqtt 1574105562562429060), entry 472 (private register): an ID25 Data-Invalid answer or a flow stale for more than 60 s stops SAT and the thermostat takes over; the 60 s limit of entry 241 stays; this replaces the keep-control part of entries 218/241. Release 2.0.0. OPEN, our interpretation (not decided by Sergeant D): when a valid flow returns, SAT re-enters through its normal start path.
<!-- SECTION:NOTES:END -->
