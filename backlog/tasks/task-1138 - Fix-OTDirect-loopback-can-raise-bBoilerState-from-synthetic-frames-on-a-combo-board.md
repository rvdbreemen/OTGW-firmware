---
id: TASK-1138
title: >-
  Fix: OTDirect loopback can raise bBoilerState from synthetic frames on a combo
  board
status: To Do
assignee: []
created_date: '2026-09-18 05:23'
labels:
  - bug
dependencies: []
priority: low
ordinal: 282000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while porting TASK-1135 and deliberately left out of that change: pre-existing, and fixing it there would have mixed two concerns.

OTDirect loopback bridges synthetic B-frames into the parser. On a combo board with HAS_PIC true, those frames write the boiler last-seen stamp like any other, so state.otBus.bBoilerState can rise from simulated traffic rather than from a real boiler.

otDirectBoilerPresent() already excludes loopback explicitly. The bBoilerState branch does not, which is the inconsistency.

TASK-1135/1137 did not introduce this and does not widen it. What that change does do is make the flag fall again once the synthetic traffic stops, where before it would have stayed raised until reboot. So the window is now bounded, which arguably makes this easier to observe than it was.

Worth deciding rather than assuming: should loopback traffic count as boiler presence at all? A bench rig may want it to, so that the rest of the stack behaves as if a boiler were there. If so this is documentation, not a defect. If not, the bBoilerState branch needs the same loopback exclusion otDirectBoilerPresent() already has.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A decision is recorded on whether loopback traffic should count as boiler presence, with the bench-simulation use case weighed explicitly
- [ ] #2 If it should not count: the bBoilerState path excludes loopback the same way otDirectBoilerPresent() does, and the two cannot drift apart again
- [ ] #3 If it should count: the asymmetry with otDirectBoilerPresent() is documented at both sites so the next reader does not file this again
- [ ] #4 Build green for the combo target and evaluate.py --quick shows no new failures
<!-- AC:END -->
