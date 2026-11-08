---
id: TASK-1734
title: >-
  Water pressure 0.1 bar: flag as no real sensor at once instead of after 10
  minutes (tjfs)
status: To Do
assignee: []
created_date: '2026-11-08T15:28:00Z'
updated_date: '2026-11-08T21:34:00Z'
labels:
  - webui
  - maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 841000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-08 15:13 #alpha-testing (msg 1569007058538201262): 'I think you are being too conservative with the pressure. no true boiler pressure can be 0.1 bar.' TASK-1682 (beta.138) flags a constant 0.10 bar for 10 minutes as a probable pressure switch. Proposal: flag 0.1 bar (or <= 0.1) immediately. Tester request: Robert decides.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides the rule (immediate vs 10-minute window, threshold)
- [ ] #2 If changed: web UI flag follows the new rule, test red/green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 18:07 Robert in #dev-sat-mqtt (msg 1569050822783017041): 'when the pressure is low and unrealistic like 0.1 it's probably not a measuring pressure but a failure from a boiler to report properly. Ask tsjf in the alpha channel what he thinks it should be called.' Reading: Robert treats a constant 0.1 bar as a boiler reporting failure, not a measurement; naming question put to tjfs in #alpha-testing (Discord agent, 2026-11-08 ~21:58).

2026-11-08 21:29 tjfs #alpha-testing (msg 1569101468550828093): 'I think you could either hide it altogether or take it as the boiler being happy with the pressure, if it gets too low the boiler should shut down'. Tester input (two options: hide the value, or show it as OK/no fault); Robert decides.

2026-11-08 ~23:10 Robert decided (AskUserQuestion in coordinator session otgw-firmware-4c): flag immediately. A reported water pressure <= 0.1 bar gets the 'no real pressure sensor (switch)' flag at once, no 10-minute window; the value shows as a dash instead of a number. Rationale: no real boiler runs at 0.1 bar (tjfs 15:13, Robert 18:07: fixed 0.1 bar is a boiler reporting failure, not a measurement). Hiding the sensor entirely was rejected (loses the switch-OK signal).
<!-- SECTION:NOTES:END -->
