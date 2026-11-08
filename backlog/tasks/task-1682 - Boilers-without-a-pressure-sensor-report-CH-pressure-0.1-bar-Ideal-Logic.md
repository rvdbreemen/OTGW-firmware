---
id: TASK-1682
title: Boilers without a pressure sensor report CH pressure 0.1 bar (Ideal Logic)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T09:03:00Z'
updated_date: '2026-11-08T22:38:00Z'
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

Lead integration 2026-11-07 on origin/dev 224a1f7ae (wt-1678): tests/webui/pressure-switch-flag.test.mjs fix 7/7 pass, old v2.js (HEAD copy via OTGW_V2_JS) 0/7; evaluate --quick 0 failed. Not yet on a device: needs a boiler that reports 0.1 bar for the field check (tjfs).

2026-11-08 superseded in part by TASK-1734 (Robert's final decision): the web UI no longer marks a fixed 0.1 bar as 'switch?'; every readout shows the decoded MsgID 18 value, like MQTT/REST/HA. The TASK-1682 helpers and the 10-minute window were removed in lane-1734 (commit 0ff5bcbb6, to be rebased). Status stays Done: the work was delivered and then deliberately reverted.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 Robert: flag in the UI. Implemented in v2.js (pressFlatSince, trackPressSwitch, pressText/setPress): 0.10 bar (+-0.005) for 10 min shows '0.10 bar · switch?' with a tooltip and drops the low-pressure class; MQTT/REST unchanged. tests/webui/pressure-switch-flag.test.mjs old 0/7, new 7/7. Pushed ad2e39816 (beta.138), bench .61 page contains the code (no boiler on the bench, so the flag itself was not seen on a device), #alpha-testing 1568913428758798336 (zip beta.141+963b68d covers 137-141).
<!-- SECTION:FINAL_SUMMARY:END -->
