---
id: TASK-1875
title: >-
  Graph history: the outside column stays at 8.3 C for 5 h while SAT's weather
  input falls to 2.0 (gruijter beta.214)
status: To Do
assignee: []
created_date: '2026-11-12T20:57:00Z'
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
