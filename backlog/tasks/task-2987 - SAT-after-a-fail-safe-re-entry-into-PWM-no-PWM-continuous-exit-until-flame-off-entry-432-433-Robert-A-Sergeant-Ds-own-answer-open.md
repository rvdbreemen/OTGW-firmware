---
id: TASK-2987
title: >-
  SAT: after a fail-safe re-entry into PWM no PWM->continuous exit until
  flame-off (entry 432/433, Robert A; Sergeant D's own answer open)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-21T01:44:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1116000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (maintainer) answered Q-2005d with A (entry 432, confirmed in entry 433, #dev-sat-mqtt 2026-11-21). Once the 3.0 C fail-safe (SATcycles.ino:1434-1439, entry 429) has switched a burning flame back to PWM, neither the underheat exit (SATcycles.ino:1494-1500, entry 342) nor the saturation exit (:1509-1517) may fire again until the flame goes out. Today both exits ignore the entry-429 flag, so the loop repeats. Field evidence: indigo_light beta.392 19 Nov 21:23-21:49, one flame with 4 switches (21:32:30 saturation exit, 21:33:30 fail-safe, 21:36:30 underheat exit, 21:39:00 fail-safe); gruijter beta.401 20 Nov, 3 flames each PWM -> continuous -> fail-safe PWM. No thermo-nova counterpart (heating_control.py:262-305, :307-376). Sergeant D's own answer to Q-2005d is still open; the wider minimal-switching proposal (margin on the entry-342 exit) is separate and goes to him first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test reproduces the indigo_light loop on the real satCycleCheckOvershootSwitch()/satCycleCheckPwmExits() code: after a fail-safe re-entry the underheat or saturation exit fires again (red on the current code)
- [ ] #2 With the fix the same test shows no PWM->continuous exit after a fail-safe re-entry until flame-off, and the first PWM->continuous exit of a flame (before any fail-safe) still works
- [ ] #3 Flame-off clears the state: the next flame can leave PWM again
- [ ] #4 Bench via the NAS (replay or simulation, per the NAS recipe) shows at most one fail-safe switch per flame
<!-- AC:END -->
