---
id: TASK-1875
title: >-
  Graph history: the outside column stays at 8.3 C for 5 h while SAT's weather
  input falls to 2.0 (gruijter beta.214)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-12T20:57:00Z'
updated_date: '2026-11-14T15:21:00Z'
labels:
  - graph
  - webui
  - bug
milestone: 2.0.0
dependencies: []
ordinal: 949000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source: gruijter's graph export otgw-graph-20261112-212749.csv (beta.214, #alpha-testing https://discord.com/channels/812969634638725140/1514720723980259460/1570536002936905839): the outside column is stuck at 8.3 from about 16:15 to 21:27 local, while SAT's own outside input (weather, SATTICK out=) falls from 8.3 to 2.0 in the same capture (20261112-121301-upload.zip). Graph code: graphHistoryStuff.ino:67-69 (origin/dev) uses state.sat.weather.fTemperature when state.sat.weather.bValid, else OTcurrentSystemState.Toutside (MsgID 27). Possibly the same as indigo_light's flat outside graph (#nederlandse-ondersteuning https://discord.com/channels/812969634638725140/815561033036333076/1570333573066653737 and 1570420467200688160). Not verified: which branch the slots took, and whether the live v2 graph shows the same. Found by the comms capture analysis 2026-11-12.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Web UI test on the real v2.js: a hidden tab drops the weather OUTSIDE value (graph gap) and a shown tab fetches it at once; red on the old v2.js
- [x] #2 Web UI test: a weather answer that turns invalid drops the old weather value; red on the old v2.js
- [x] #3 Bench: on a unit with weather, the v2 page drops OUTSIDE when hidden and has it back right after it is shown
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Cause (comms analysis 2026-11-13, confirmed in code): fetchWeather() skips a hidden tab (TASK-1741) and the visibilitychange handler only cleared model.wxValid, so model.outside kept the last weather value and pushGraphSlot() copied it into every graph slot; a weather answer with valid:false did the same. Fix in data/v2.js: dropWeatherOutside() clears model.outside when it came from the weather API (wxValid) and clears wxValid; used on hide and on an invalid weather answer. On every show (also after < 15 s) fetchWeather runs again, also when the SAT page is open (its own weather call only renders the SAT card). MsgID 27 from the bus still fills OUTSIDE when weather is not valid (v2.js case 27). Test: tests/webui/outside-weather-dropped.test.mjs on the real v2.js.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 reproduced on beta.218 with a likely cause (gruijter capture, analysis 1570826381443469447). Graph outside column flat at 6.5 from 11:14 to 12:55 while SAT moved 7.0->7.6, and at 9.2 from 13:49 to 16:05 while SAT moved 9.5->8.4. Telnet: the second browser poll of /api/v2/sat/weather (at :09 each minute) stops after 11:21:09 and reappears only at 12:55:18-19 and 16:05:49; the graph changes at 12:55:20 and 16:05:50. Code verified by comms on origin/dev: fetchWeather() returns early when document.hidden (v2.js:5630-5631); the visibilitychange handler sets model.wxValid=false but keeps model.outside (v2.js ~8506-8508), so the stale value keeps being recorded, while the comment v2.js:8502-8505 says weather values are dropped and the graph shows a gap. Inference, not in the log: the tab was hidden. No MsgID 27 on the bus, so nothing else replaces model.outside (v2.js:2320 only on a MsgID 27 Read-Ack). SAT itself used a valid weather value throughout (osrc=W).

2026-11-14 (09): tests/webui/outside-weather-dropped.test.mjs on the real v2.js/v2.html: with OTGW_DATA_DIR = origin/dev data/ both tests FAIL ('hidden: OUTSIDE is 8.3, not dropped'; 'an invalid weather answer left OUTSIDE at 6.5'); on the fix both PASS. Related web UI tests one file at a time, Chrome 0 before and after: poll-spread-hidden 5/5, graph-control-setpoint-source 11/11, rest-503-keeps-state 3/3, health-chain-one-in-flight 2/2, dallas-discovery-toast 3/3. evaluate.py --quick 0 failed.

2026-11-14 (09) coordinator checks: (1) every weather fetch goes through the v2 API queue (ADR-184, v2.js wraps window.fetch, MAX_IN_FLIGHT 2); fetchWeather now also skips while its previous request is still open (wxBusy). New test case: 20 hide/show toggles give a network peak of at most 2 requests in flight and at most 2 weather requests, OUTSIDE back afterwards (a guard for this change; the old code fetched nothing on a short show, so it is no red-on-old case). (2) ADR-193 device ring (graphHistoryStuff.ino:67-69) takes the outside column from the firmware's own weather fetch (state.sat.weather), independent of any browser tab, so the hidden-tab bug does not reach it; in gruijter's capture SAT moved with valid weather (osrc=W). Out of scope here, filed separately: state.sat.weather.bValid is never set back to false, so the ring would keep the last weather value if the firmware's own fetch stopped (by reading only).

2026-11-14 (09) bench .63 with headless Chrome on the device's own v2 page (tools/wxhide.mjs: document.hidden override + visibilitychange, then window.OTGWv2.model): old 246 visible {outside 7.5, wxValid true} -> hidden {7.5, false} KEPT -> shown {7.5, false}, no refresh after 5.3 s. beta.247+f36ca2a, 2 runs: hidden {null, false} DROPPED (gap) -> shown {7.5, true} after 0.8 s and 1.0 s. Right after the OTA the firmware had no weather yet (valid:false, age -1), so the bench ran after its first fetch. Health: bootcount 119, hash-check f36ca2a, settings diff [] (API and raw ini), crashlog unchanged. Pushed as 6d82f3927 (src identical to f36ca2a after two docs-only rebases); its zip is covered by the next post (coordinator).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: the hidden-tab handler and an invalid weather answer only cleared wxValid; model.outside kept the last weather value and the graph recorded it. Fix (v2.js): dropWeatherOutside() empties the weather-derived OUTSIDE (gap; MsgID 27 still fills it), every show refetches the weather, one weather request at a time. AC1-2: tests/webui/outside-weather-dropped.test.mjs red on the old v2.js, green now (plus a hide/show storm guard: peak <= 2 in flight). AC3: bench .63, old 246 kept 7.5 while hidden, 247 dropped it and had it back within 1 s of showing. Out of scope: the device ring (TASK-1937).
<!-- SECTION:FINAL_SUMMARY:END -->
