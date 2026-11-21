---
id: TASK-3021
title: >-
  SAT: frost guard when SAT is off and no thermostat is connected (room <= 6 C
  -> 30 C flow until 7 C)
status: To Do
assignee: []
created_date: '2026-11-21T18:47:00Z'
labels:
  - sat
  - safety
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1135000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: entry 460 in the private SAT register (Robert, 2026-11-21, Q-456a = B). When SAT is switched off and no thermostat is detected behind the gateway (OT-Direct master mode without a thermostat), today the gateway sends CH enable off with no CS override (OTDirect.ino:1259-1271), so nothing protects the house from freezing. With a thermostat, SAT off hands back (satSendRelease SATcontrol.ino:882-888) and the thermostat's frost protection applies: unchanged. Reference: Alex ba515c8 demand_control.py:610-622, :815-828, const.py:31-32. Room temperature from the room source of entries 457/458; no room temperature known -> no action. Open per entry 460: fixed 6 C or a setting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With SAT off, no thermostat detected and room <= 6.0 C, the gateway sends CH enable on with a 30 C flow setpoint; at room >= 7.0 C it returns to CH off
- [ ] #2 With a thermostat connected, behaviour is unchanged (hand-back only)
- [ ] #3 No room temperature known -> no frost demand
- [ ] #4 Host test on the real code: red on the old code (no demand at 5 C), green on the fix; bench or simulation check on OT-Direct master mode
<!-- AC:END -->
