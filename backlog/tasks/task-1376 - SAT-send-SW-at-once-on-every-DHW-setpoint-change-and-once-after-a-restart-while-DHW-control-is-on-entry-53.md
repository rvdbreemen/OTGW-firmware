---
id: TASK-1376
title: >-
  SAT: send SW= at once on every DHW setpoint change and once after a restart,
  while DHW control is on (entry 53)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-25T08:29:00Z'
updated_date: '2026-10-30T09:13:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 511000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 53 (2026-10-25, answer to Q17: A). Entry 33: a DHW setpoint change sends SW={value}. Firmware today sends SW= only on the off-to-on edge of a hot water draw and only with bDhwEnabled (SATcontrol.ino:4885-4891 at HEAD 31933adaf). thermo-nova sends at once on change (number.py:72-74). Change: send SW=<int> when the setting changes (REST/MQTT/UI) and once after boot/SAT enable, while bDhwEnabled; keep the draw-edge send or drop it (decide in plan). Note entry 53: users set it once and forget it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A change of dhw_setpoint while DHW control is on sends SW=<value> once, at once (host test red on HEAD, green on fix)
- [ ] #2 After a restart with DHW control on, SW= is sent once when the command interface is up
- [ ] #3 No SW= while DHW control is off
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: Sergeant D entry 69 (2026-10-27, Q17: yes) re-confirms entries 33 and 53: SW= at once on every change and once after a restart while DHW control is on; slider bounded by MsgID 48. No change to scope. Open (from entry 69): whether the SW= on a draw edge also stays; decide in the plan.

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Robert 2026-10-30: SW= sync only while SAT is enabled (not when SAT is off with DHW control on). Plan drafted: satDhwSetpointSync() after satRestartStep() (strict entry 121 reading), drop the draw-edge send if Sergeant D agrees.
<!-- SECTION:NOTES:END -->
