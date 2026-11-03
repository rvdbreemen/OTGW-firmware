---
id: TASK-1546
title: >-
  Web UI graph: draw a room-temperature dropout as an interruption line in a
  different colour instead of holding the last value
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T15:56:00Z'
updated_date: '2026-11-03T02:54:00Z'
labels:
  - web-ui
  - sat
dependencies: []
priority: medium
ordinal: 701000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01, from gruijter's alpha.597 capture (#alpha-testing msg 1566431444719509578, answered in 1566479847427936411). He unplugged the thermostat 11:20:58-12:20:39. SAT marked the room temperature invalid at 11:25:33 (SATTICK room=nan rv=0) and ran curve-only, but the Home graph kept drawing room = 19.34 C for the whole hour, because the graph samples model.room every tick (v2.js:1350) and model.room keeps the last MsgID 24 value. The CSV export (v2.js:3491) shows the same frozen value. Wanted: while the room temperature is not valid, the room series shows an interruption line in a different colour (not the green room colour), so it is clear there was no reading; the CSV marks those samples as empty. Validity source to decide during the plan: SAT's room-valid state when SAT is enabled (sat/status room_temp null / rv=0), otherwise MsgID 24 staleness (same window as SAT's room timeout, ~4.5 min observed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 While the room temperature is invalid the graph draws the room series as an interruption line in a different colour from the room line; valid stretches stay unchanged
- [x] #2 CSV export writes an empty room value for invalid samples
- [x] #3 Browser test on the real page: red on the old page (frozen value drawn), green on the new one, screenshots before/after with synthetic data
- [x] #4 Works with SAT on and with SAT off (no thermostat MsgID 24 for longer than the timeout)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: pushGraphSlot (v2.js:1350) copies model.room without validity. Agent-doable.

2026-11-03 (agent, verified by lead incl. screenshot shots/1546-new/dropout-2-sat-off.png): room line breaks during a dropout and a dotted #ff6b9d interruption line runs along the bottom; CSV writes an empty room value. Lost = SAT room_temp_lost (SAT on) or MsgID 24 silent > 270 s (SAT off). New tests/webui/graph-room-dropout.test.mjs (4 scenarios): old v2.js 6 FAIL, fix 0; graph-control-setpoint-source 11/11, graph-break 8/8, graph-history-merge 18/18. Limit: the gateway 24 h history (graphHistoryStuff.ino) keeps Tr as 'seen ever', so after a reload the history part shows the held value -> follow-up task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The Monitor graph draws a room-temperature dropout as an interruption (room line stops, dotted pink line along the bottom) instead of a held value, with SAT on (room_temp_lost) and off (MsgID 24 silent > 270 s); the CSV has an empty room value there. Evidence: new browser test graph-room-dropout (old v2.js 6 FAIL, fix 0), the other graph tests green, screenshots before/after. Not covered: the gateway-side 24 h history, which still holds the last Tr (follow-up task).
<!-- SECTION:FINAL_SUMMARY:END -->
