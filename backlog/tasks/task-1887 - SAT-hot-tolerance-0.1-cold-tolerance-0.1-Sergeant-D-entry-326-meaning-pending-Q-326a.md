---
id: TASK-1887
title: >-
  SAT: hot tolerance 0.1 / cold tolerance -0.1 (Sergeant D entry 326, meaning
  pending Q-326a)
status: To Do
assignee: []
created_date: '2026-11-13T07:25:00Z'
labels:
  - sat
  - blocked
milestone: 2.0.0
dependencies: []
ordinal: 960000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D decision 2026-11-13 (https://discord.com/channels/812969634638725140/1105556725714649128/1570698600952893454), entry 326. No tolerance setting exists for SAT in the firmware; thermo-nova has no hot/cold tolerance key (only DEADBAND 0.1, const.py:16); HA MQTT climate has no such option. Blocked on Q-326a (asked https://discord.com/channels/812969634638725140/1105556725714649128/1570700362187542539): A hysteresis on heating demand / B PID-self-tune deadband / C HA display only. SAT control change if A or B: plan with citations and Sergeant D's OK before implementation.
<!-- SECTION:DESCRIPTION:END -->
