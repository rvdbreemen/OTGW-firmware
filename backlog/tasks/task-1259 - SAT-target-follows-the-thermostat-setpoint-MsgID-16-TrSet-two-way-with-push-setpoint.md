---
id: TASK-1259
title: >-
  SAT target follows the thermostat setpoint (MsgID 16 TrSet), two-way with
  push-setpoint
status: To Do
assignee: []
created_date: '2026-10-19 17:33'
updated_date: '2026-10-20 02:17'
labels:
  - feature
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 391000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by gruijter 2026-10-19, endorsed by Sergeant D. Today SAT only pushes its target to the thermostat (satpushsetpoint, TC= command, SATcontrol.ino ~407-415); a setpoint change on the wall thermostat is ignored, so the thermostat is useless as UI. Request: opt-in 'SAT target follows thermostat setpoint' that takes MsgID 16 TrSet as SAT target when it changes. Must avoid a feedback loop with satpushsetpoint (TC= echo). Sergeant D's rule: thermostat syncs only the room setpoint; with SAT active SAT drives the boiler; on SAT inactive/malfunction fall back to the thermostat. Open: thermostat may send CH enable off when it thinks the room is warm, decide who wins.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design agreed (option name, loop guard, CH-enable arbitration)
- [ ] #2 Thermostat setpoint change is adopted as SAT target within one TrSet cycle, and a SAT target change pushed via TC= does not bounce back (bench or simulator evidence)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-19 Sergeant D (#alpha-testing): adopt the thermostat setpoint only on a room setpoint CHANGE (edge-triggered), not by periodic sync. CH-enable arbitration question still open.

2026-10-20 maintainer decision: CH enable from the thermostat is ignored while SAT is active and healthy (SAT wins); when SAT is off or faulted the thermostat applies again.
<!-- SECTION:NOTES:END -->
