---
id: TASK-1937
title: >-
  Device graph ring keeps the last weather value when the firmware's weather
  fetch stops (bValid never cleared)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T14:45:00Z'
updated_date: '2026-11-14T17:04:00Z'
labels:
  - bug
  - graph
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 992000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
By reading (09, TASK-1875 review): graphHistoryStuff.ino:67-69 stores state.sat.weather.fTemperature in the 24 h ring (ADR-193) whenever state.sat.weather.bValid is true, and bValid is only ever set to true (SATweather.ino:529); nothing clears it when the weather fetch stops working. SATcontrol uses an age check (wxFresh, SATcontrol.ino:~2700), the ring does not, so after a weather outage the ring records the last value as if it were current. Not seen in the field yet; not reproduced.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: with the weather fetch failing for longer than its refresh age, the ring keeps writing the old weather value (host test or bench)
- [ ] #2 Fix: the ring uses the same freshness rule as SAT (or MsgID 27 when the weather value is stale); red-then-green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Display only, no SAT behaviour change: the ring uses SAT's weather freshness rule (SATcontrol.ino:2700, bValid + timestamp + younger than SAT_OUTSIDE_VALID_MS = 1 h, Sergeant D entry 57), else MsgID 27. SATcontrol untouched (SAT ground-truth rules). Host harness on the real slice of graphHistoryTick() for old vs fix.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix: graphHistoryTick() (graphHistoryStuff.ino) stores the weather value only when bValid && iLastUpdateMs != 0 && younger than GRAPH_HIST_WEATHER_VALID_MS (graphHistoryStuff.h, 3600000 UL, the same value as SAT_OUTSIDE_VALID_MS; a test keeps both equal); else MsgID 27 as before. New tests/test_graph_outside_source.py compiles the real outside-source slice of graphHistoryTick() with the revision's real graphHistoryStuff.h and jsonEmit.h: OLD (origin/dev) red on stale_ot (a 2 h old weather value is written, MsgID 27 ignored), stale_none and valid_no_stamp; FIX green on all (fresh, 59 min edge, stale -> MsgID 27, stale without 27 -> no value, never fetched, no timestamp) plus the window-equality check. Existing tests/test_graph_history.cpp (built by hand, it has no runner): ALL TESTS PASSED. test_static_integrity, test_ino_prototype_hoisting PASS; evaluate --quick 85/0. Not changed: v2.js's live graph (it reads the API's weather 'valid' flag; TASK-1875 already drops a stale weather OUTSIDE in the UI). AC1 ticked (host reproduction); AC2 after the build.

Rebased onto origin/dev (beta.248 published by TASK-1918): code identical (146 lines); bump beta.248 -> beta.249 in its own commit.

RED on .61 by 09 (2.0.0-beta.240+a3dff10, old code), 2026-11-14: satweatherenable true, weather valid, W 5.3 C, last fetch about 16:45:42; 16:52:14 satweatherenable false (valid stays true). 18:02:09, 76 min after the last fetch: /api/v2/sat/weather valid true, temperature 5.3, age 4582 s; /api/v2/graph/history newest 10 outside all 53 (5.3 C), the whole ring (120 slots, last hour) only [53], 0 nulls. No MsgID 27 on .61 (no boiler), so the fix should give null from about 1 h after the last fetch. Settings and crashlog unchanged. Evidence: 09 scratchpad r1937/. Build beta.249+45187f4 (18:03, 9m44s, RTC gate OK) for GREEN; archived in OTGW-build-archive/beta.249-1937.
<!-- SECTION:NOTES:END -->
