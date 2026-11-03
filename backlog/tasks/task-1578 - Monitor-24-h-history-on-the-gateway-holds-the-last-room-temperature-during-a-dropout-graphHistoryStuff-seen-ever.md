---
id: TASK-1578
title: >-
  Monitor 24 h history on the gateway holds the last room temperature during a
  dropout (graphHistoryStuff 'seen ever')
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-03T02:54:00Z'
updated_date: '2026-11-03T13:50:00Z'
labels:
  - web-ui
  - sat
dependencies: []
priority: low
ordinal: 724000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found with TASK-1546 (2026-11-03): the live graph now breaks the room line on a dropout, but the gateway's own 24 h history (src/OTGW-firmware/graphHistoryStuff.ino ~line 62, ADR-193) records Tr as 'seen ever', so after a page reload the history-loaded part of the graph still shows the frozen room value. Make the history slot record a missing room temperature (same rule as the live graph: SAT room_temp_lost, or MsgID 24 silent > 270 s), within the ADR-193 slot format.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A history slot taken during a room dropout stores 'no value' for Tr; GET history returns it as such
- [x] #2 After a reload the graph shows the same interruption as the live graph (browser test with the fake device history)
- [x] #3 Host test on the real graphHistoryStuff code, red on the old code; ADR-193 slot size unchanged or the ADR updated
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 (agent, verified by lead): graphHistEncRoom()/graphHistRoomLost() in graphHistoryStuff.h: a slot stores GRAPH_HIST_NO_TEMP (INT16_MIN, JSON null) for Tr when SAT reports room_temp_lost or MsgID 24 was seen but is silent > 270 s (the TASK-1546 live rule); 14-byte slot unchanged, GET history output unchanged. v2.js mergeGraphHistory flags a null room slot as roomX when the window has room readings. tests/test_graph_history.cpp 7 new checks PASS (old header does not compile: helper missing); graph-history-merge 20/20 (old v2.js 1 FAIL); graph-room-dropout scenarios 5-6 (old v2.js 2 FAIL); all 6 graph tests green; evaluate 0 failed. Build (static helper visibility across .ino) and device not yet verified.
<!-- SECTION:NOTES:END -->
