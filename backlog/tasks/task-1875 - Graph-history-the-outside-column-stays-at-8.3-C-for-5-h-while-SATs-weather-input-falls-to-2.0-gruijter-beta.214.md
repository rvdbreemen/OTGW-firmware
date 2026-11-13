---
id: TASK-1875
title: >-
  Graph history: the outside column stays at 8.3 C for 5 h while SAT's weather
  input falls to 2.0 (gruijter beta.214)
status: To Do
assignee: []
created_date: '2026-11-12T20:57:00Z'
updated_date: '2026-11-13T15:47:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 reproduced on beta.218 with a likely cause (gruijter capture, analysis 1570826381443469447). Graph outside column flat at 6.5 from 11:14 to 12:55 while SAT moved 7.0->7.6, and at 9.2 from 13:49 to 16:05 while SAT moved 9.5->8.4. Telnet: the second browser poll of /api/v2/sat/weather (at :09 each minute) stops after 11:21:09 and reappears only at 12:55:18-19 and 16:05:49; the graph changes at 12:55:20 and 16:05:50. Code verified by comms on origin/dev: fetchWeather() returns early when document.hidden (v2.js:5630-5631); the visibilitychange handler sets model.wxValid=false but keeps model.outside (v2.js ~8506-8508), so the stale value keeps being recorded, while the comment v2.js:8502-8505 says weather values are dropped and the graph shows a gap. Inference, not in the log: the tab was hidden. No MsgID 27 on the bus, so nothing else replaces model.outside (v2.js:2320 only on a MsgID 27 Read-Ack). SAT itself used a valid weather value throughout (osrc=W).
<!-- SECTION:NOTES:END -->
