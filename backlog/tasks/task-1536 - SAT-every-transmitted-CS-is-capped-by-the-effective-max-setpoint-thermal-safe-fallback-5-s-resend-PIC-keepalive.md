---
id: TASK-1536
title: >-
  SAT: every transmitted CS is capped by the effective max setpoint
  (thermal-safe fallback, 5 s resend, PIC keepalive)
status: To Do
assignee: []
created_date: '2026-11-01T00:01:00Z'
labels:
  - sat
  - low-load
dependencies: []
priority: medium
ordinal: 692000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.2 second read (2026-11-01): the thermal-safe fallback caps CS with satGetMaxSetpoint() (user MaxTSet only, SATcontrol.ino:6515-6522) instead of satEffectiveMaxSetpoint() (:405, which also includes the boiler's own MaxTSet), and the 5 s resend (:634-641) and the PIC CS keepalive (:597-606) repeat the last CS without clamping it again after a limit change. Related: TASK-1412.13 (MaxTSet negotiation and every transmitted ceiling), entries 48/51/54/67/100/110/148/152.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: thermal-safe fallback, resend and keepalive never send a CS above satEffectiveMaxSetpoint(), also right after the limit is lowered (red on HEAD, green)
- [ ] #2 Decision check against entries 148/152 recorded in the notes before the change
<!-- AC:END -->
