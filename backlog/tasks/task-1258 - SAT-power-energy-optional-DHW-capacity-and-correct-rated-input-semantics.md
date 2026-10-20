---
id: TASK-1258
title: 'SAT power/energy: optional DHW capacity and correct ''rated input'' semantics'
status: To Do
assignee: []
created_date: '2026-10-19 17:33'
updated_date: '2026-10-20 02:18'
labels:
  - feature
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 390000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by gruijter 2026-10-19 (Intergas HReco 30). sat/power = RelModLevel/100 x SATboilercapacity while the flame is on (SATcontrol.ino ~2963-2976), for CH and DHW alike. Many boilers report modulation relative to the active max (CH limit ~12 kW, DHW limit ~23 kW), so DHW is under-counted. Request: optional 'DHW capacity' used when the DHW-active status bit is set; same for the SATboilerratedkw gas estimate. Also: UI label 'Boiler rated output' (v2.js ~1792) while SATtypes.h documents it as rated input; the gas estimate multiplies rated x efficiency (heat output, not gas input) and has a flow-based branch using DHWFlowRate (MsgID 19). Needs design review before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design decision recorded (DHW capacity setting yes/no, label, gas-estimate formula)
- [ ] #2 If implemented: sat/power uses DHW capacity while DHW is active, host test shows CH and DHW cases
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 Sergeant D (#alpha-testing): Intergas boilers ignore the max-modulation command (MM=); output is limited through the boiler's own parameters (CH limit param 3, DHW limit param 4). So modulation % is relative to whichever limit is active, which supports a separate DHW capacity setting rather than deriving it from MM.

2026-10-20 maintainer decision (full package): a) optional setting 'DHW capacity' (0 = use boiler capacity) used while the DHW-active status bit is set, for sat/power and the gas estimate; b) UI label 'Boiler rated output' -> 'Boiler rated input', setting key unchanged; c) drop the DHWFlowRate (MsgID 19) branch from the gas estimate, keep modulation x rated input.
<!-- SECTION:NOTES:END -->
