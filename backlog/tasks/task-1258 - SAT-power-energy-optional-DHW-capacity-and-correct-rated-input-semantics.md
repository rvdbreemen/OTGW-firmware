---
id: TASK-1258
title: 'SAT power/energy: optional DHW capacity and correct ''rated input'' semantics'
status: To Do
assignee: []
created_date: '2026-10-19 17:33'
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
