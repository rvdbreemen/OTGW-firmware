---
id: TASK-1841
title: Home Mission control and SAT control loop graphs in the Monitor Graph style
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:52:00Z'
updated_date: '2026-11-12T08:08:00Z'
labels:
  - webui
  - graph
dependencies: []
priority: medium
ordinal: 917000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12: three graphs exist: Home Mission control (strip chart #strip), SAT control loop (#satHistChart) and Monitor Graph (TASK-1821 stacked panels). Apply the Monitor Graph style to the other two with the information that fits each screen; only Monitor Graph carries all data. Style: stacked panels on one time axis, panel titles, own y axes with units, on/off lanes as areas, legend, light/dark, translated.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Mission control strip uses the stacked-panel style with the series that fit its purpose (decision recorded in the notes)
- [x] #2 SAT control loop chart uses the stacked-panel style with the SAT loop data, decision recorded
- [x] #3 Tests red on the old code; screenshots of both, light and dark
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Parked (Robert via coordinator 2026-11-12: HIGH firmware first, 1841 waits). WIP patch .git/wip-task-1841.patch (copy in the session scratchpad): drawPanelChart() shared renderer in the Monitor > Graph style; Mission control strip = flame lane, modulation, temperatures (flow, return, control setpoint as a series, flame added to samples); SAT control loop = room/outside, setpoint/heating curve, PID (no unit claimed); old strip elements, .ser-* CSS and home.chart_flow/chart_ret keys removed. State: panel-chart-style.test.mjs red 2/2 on the old code; with the patch 1 of 2 green, the geometry test still failing (to finish); sat-page-order, sat-beta-notice, v2-undefined-names, nested-helper-scope, i18n green. No screenshots yet.

Resumed in worktree wt-ba-ui on origin/dev (after the beta.191-197 batch): WIP patch applied cleanly (3-way). The remaining red test was the test itself: its brace matcher skips strings but not regex literals, and v2.js esc() has /"/g; esc is now restated in the test (same three replacements), drawPanelChart stays the real one. panel-chart-style.test.mjs: old code 2 FAIL, fix 2 OK. Neighbours OK: v2-undefined-names 2, nested-helper-scope 2, i18n-lang 6, i18n-no-hardcoded 3, sat-page-order 1, sat-beta-notice 1, graph-panels 7. Headless Chrome with a real WebSocket streaming synthetic boiler frames: Mission control strip viewBox 780x256, panels flame/modulation/temperatures, 4 paths, 0 exceptions (screenshot scratchpad bench/c1841-mission.png); SAT control loop viewBox 780x302, panels 'Room · Outside', 'Setpoint · Flow', 'PID' drawn (partial screenshot bench/c1841-sat.png; full light/dark screenshots wait for a free Chrome slot, 09's 1763 run holds it).

Decisions (AC1/AC2): Mission control = the boiler loop right now: flame on/off lane, modulation 0-100 %, temperatures flow + return + control setpoint as a series (was a flat line at the last value). SAT control loop = room + outside, setpoint + heating curve, PID output on its own scale with no unit (unit not checked in the SAT source, so none claimed). Full history stays on Monitor > Graph.

Screenshots 2026-11-12 ~04:50, real v2 page in headless Chrome, fake device streaming synthetic boiler frames over a real WebSocket and a changing sat/status: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/c1841-dark-mission.png, c1841-dark-sat.png, c1841-light-mission.png, c1841-light-sat.png. Measured: strip viewBox 780x256, titles flame/modulation/temperatures, 4 paths; SAT chart viewBox 780x302, titles 'Room · Outside'/'Setpoint · Flow'/'PID', 5 paths; 0 page exceptions in both themes; light run data-theme=light, body rgb(241,247,248). Mission control stays dark in light theme by the existing .mc instrument palette (v2.css:268, already on origin/dev, not changed here); the SAT chart follows the theme. Tests: panel-chart-style.test.mjs old 2 FAIL / fix 2 OK.

Bench 2026-11-12 by otgw-bench-tester (09) on 2.0.0-beta.208+e449692 (covers beta.204-208), .61 OTA app+fs, githash checked, settings unchanged: TASK-1841 PASS. 0 console errors on Home and SAT in light and dark (exceptionThrown + console error). Mission control: three panels with title and own y axis (flame on/off, modulation 100%/0%, temperatures 90/50/10 °C). SAT control loop: 'Room · Outside' (°C), 'Setpoint · Flow' (°C), 'PID' (no unit). Screenshots 09 scratchpad b208/ui61-{mission,satloop}-{light,dark}.png. Finding: Mission control (.mc, v2.css:268, pre-existing) keeps a fixed dark palette in the light theme; filed as TASK-1863 (needs Robert's choice), not part of this task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Home > Mission control and SAT > SAT control loop now draw with one shared drawPanelChart() in the Monitor > Graph stacked-panel style (2.0.0-beta.207, pushed in ef9aabf4c). AC1: Mission control = flame lane, modulation, temperatures (flow, return, control setpoint); decision in the notes; bench 09 on beta.208: three titled panels with own axes on .61. AC2: SAT control loop = Room/Outside, Setpoint/Flow, PID (no unit claimed); bench 09: three panels as listed. AC3: panel-chart-style.test.mjs old code 2 FAIL, fix 2 OK (rerun after the rebase: 2 pass); neighbours graph-panels 7, sat-page-order 1, sat-beta-notice 1, v2-undefined-names 2, nested-helper-scope 2; screenshots light and dark from the headless harness (session scratchpad bench/c1841-*.png) and from 09 on the real unit; 0 console errors. Follow-up: TASK-1863 (Mission control stays dark in the light theme, pre-existing).
<!-- SECTION:FINAL_SUMMARY:END -->
