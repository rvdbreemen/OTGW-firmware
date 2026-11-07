---
id: TASK-1682
title: Boilers without a pressure sensor report CH pressure 0.1 bar (Ideal Logic)
status: To Do
assignee: []
created_date: '2026-11-07T09:03:00Z'
updated_date: '2026-11-07T09:37:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 803000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs in #alpha-testing 2026-11-07 08:59 UTC (msg 1568550441648918672): some boilers such as Ideal Logic report MsgID 18 as 0.1 bar because they only have a low-pressure switch. Tester input; whether the UI/HA should treat a fixed 0.1 bar as 'no sensor' is Robert's call.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision by Robert: show as-is, flag, or hide a constant 0.1 bar
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 Robert's decision (AskUserQuestion in session 74b8fc81): flag it in the web UI. When the CH pressure stays at exactly 0.1 bar for a long time, the web UI adds 'probably a pressure switch, no sensor'. The value stays visible; Home Assistant/MQTT unchanged.
<!-- SECTION:NOTES:END -->
