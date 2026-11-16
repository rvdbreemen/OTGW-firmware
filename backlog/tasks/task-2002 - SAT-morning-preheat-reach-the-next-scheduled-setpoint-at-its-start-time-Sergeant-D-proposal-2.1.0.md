---
id: TASK-2002
title: >-
  SAT: morning preheat, reach the next scheduled setpoint at its start time
  (Sergeant D proposal, 2.1.0)
status: To Do
assignee: []
created_date: '2026-11-16T12:54:00Z'
labels:
  - needs-maintainer-ok
  - sat
  - feature
milestone: 2.1.0
dependencies: []
ordinal: 1024000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Proposed by Sergeant D for a release after 2.0.0, #dev-sat-mqtt https://discord.com/channels/812969634638725140/1105556725714649128/1571866919152128041 (2026-11-16 12:37 UTC): 'Morning Preheat. Since we will develop the weekly planner and SAT knows when the new setpoint change will occur we can develop the preheat feature. So let's say in the night the setpoint is 18.5, the day setpoint is 20 at 07:30 and the Morning preheat is enabled. SAT will start heating the home at about 5:10 so the home will be at 20 exactly at 7:30.' Depends on the weekly scheduler task. Related research: archived TASK-1566 (anticipation feasibility). Robert decides scope and priority.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design agreed with Sergeant D (how SAT estimates the start time, learning, limits) and recorded as entries
- [ ] #2 Robert's go recorded
- [ ] #3 Implementation with host tests red then green and a field or simulation proof that the room reaches the setpoint at the scheduled time
<!-- AC:END -->
