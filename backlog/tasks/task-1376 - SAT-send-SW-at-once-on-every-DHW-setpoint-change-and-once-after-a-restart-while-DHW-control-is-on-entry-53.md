---
id: TASK-1376
title: >-
  SAT: send SW= at once on every DHW setpoint change and once after a restart,
  while DHW control is on (entry 53)
status: To Do
assignee: []
created_date: '2026-10-25T08:29:00Z'
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
