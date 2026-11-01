---
id: TASK-1546
title: >-
  Web UI graph: draw a room-temperature dropout as an interruption line in a
  different colour instead of holding the last value
status: To Do
assignee: []
created_date: '2026-11-01T15:56:00Z'
labels:
  - web-ui
  - sat
dependencies: []
priority: low
ordinal: 701000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01, from gruijter's alpha.597 capture (#alpha-testing msg 1566431444719509578, answered in 1566479847427936411). He unplugged the thermostat 11:20:58-12:20:39. SAT marked the room temperature invalid at 11:25:33 (SATTICK room=nan rv=0) and ran curve-only, but the Home graph kept drawing room = 19.34 C for the whole hour, because the graph samples model.room every tick (v2.js:1350) and model.room keeps the last MsgID 24 value. The CSV export (v2.js:3491) shows the same frozen value. Wanted: while the room temperature is not valid, the room series shows an interruption line in a different colour (not the green room colour), so it is clear there was no reading; the CSV marks those samples as empty. Validity source to decide during the plan: SAT's room-valid state when SAT is enabled (sat/status room_temp null / rv=0), otherwise MsgID 24 staleness (same window as SAT's room timeout, ~4.5 min observed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 While the room temperature is invalid the graph draws the room series as an interruption line in a different colour from the room line; valid stretches stay unchanged
- [ ] #2 CSV export writes an empty room value for invalid samples
- [ ] #3 Browser test on the real page: red on the old page (frozen value drawn), green on the new one, screenshots before/after with synthetic data
- [ ] #4 Works with SAT on and with SAT off (no thermostat MsgID 24 for longer than the timeout)
<!-- AC:END -->
