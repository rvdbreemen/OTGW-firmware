---
id: TASK-1311
title: 'SAT page temperature history duplicates the Monitor graph: merge or drop it'
status: To Do
assignee: []
created_date: '2026-10-23T07:52:00Z'
updated_date: '2026-10-23T11:33:00Z'
labels:
  - webui
  - sat
  - graph
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 434000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:47 (#alpha-testing): the SAT 'Temperature history' chart doubles the Monitor graph; is there a reason to keep it? Facts: the SAT chart (v2.js satHistPush/satDrawHistory, client-side since page load, TASK-986) plots Setpoint (SAT target), Flow, Room, Outside and the PID output; the Monitor graph (TASK-1226/1296) has flow, return, control setpoint, thermostat setpoint, room, outside, modulation and flame/DHW/CH plus device-side history, but not the SAT target or the PID output. Options: (a) drop the SAT chart and add 'SAT target' and 'PID output' series to the Monitor graph (shown when SAT is enabled), with a link from the SAT page; (b) keep a small SAT chart that reuses the Monitor graph renderer and its history. Decide, then implement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision (merge into the Monitor graph, or keep a reduced SAT chart on the shared renderer) recorded in the task with the reason
- [ ] #2 No information is lost: SAT target and PID output are visible in the remaining graph when SAT is enabled (screenshot)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23: depends on the navigation concept decision in TASK-1312 (maintainer); skipped by the cycle until that decision is recorded, to avoid moving the SAT controls twice.

2026-10-23: navigation concept decided in TASK-1312 (option A: top tabs everywhere, collapsible grouped sections inside a page); this task is unblocked and follows that concept.
<!-- SECTION:NOTES:END -->
