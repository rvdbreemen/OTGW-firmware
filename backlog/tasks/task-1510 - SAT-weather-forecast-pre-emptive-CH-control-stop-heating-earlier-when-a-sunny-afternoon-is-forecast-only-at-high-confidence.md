---
id: TASK-1510
title: >-
  SAT: weather-forecast pre-emptive CH control (stop heating earlier when a
  sunny afternoon is forecast), only at high confidence
status: To Do
assignee: []
created_date: '2026-10-30T13:58:00Z'
labels:
  - needs-maintainer-ok
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 666000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request from tester gruijter in #alpha-testing (msg 1565717631510646795, 2026-10-30 13:22 UTC): 'Use weather forecast for pre-emptive CH control. But only when it can be done with high confidence!' Context: his capture of 2026-10-30 showed a sunny afternoon that a forecast could have anticipated (msg 1565715107529629787); he does this today with a Homey flow. Robert prompted the request (msg 1565717125086052435). Sergeant D (msg 1565715491010379868): 'You can work on this feature. I am not going to waste a single minute into this' (read as: not part of his SAT design, no input from him). thermo-nova has no forecast-based control (to verify). Needs a design: which forecast signal (solar radiation / cloud cover / temperature trend from the existing weather fetch), what confidence means, how it interacts with PID, solar gain and presets. Not a beta-1 item.
<!-- SECTION:DESCRIPTION:END -->
