---
id: TASK-1841
title: Home Mission control and SAT control loop graphs in the Monitor Graph style
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:52:00Z'
updated_date: '2026-11-12T03:05:00Z'
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
- [ ] #1 Mission control strip uses the stacked-panel style with the series that fit its purpose (decision recorded in the notes)
- [ ] #2 SAT control loop chart uses the stacked-panel style with the SAT loop data, decision recorded
- [ ] #3 Tests red on the old code; screenshots of both, light and dark
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Parked (Robert via coordinator 2026-11-12: HIGH firmware first, 1841 waits). WIP patch .git/wip-task-1841.patch (copy in the session scratchpad): drawPanelChart() shared renderer in the Monitor > Graph style; Mission control strip = flame lane, modulation, temperatures (flow, return, control setpoint as a series, flame added to samples); SAT control loop = room/outside, setpoint/heating curve, PID (no unit claimed); old strip elements, .ser-* CSS and home.chart_flow/chart_ret keys removed. State: panel-chart-style.test.mjs red 2/2 on the old code; with the patch 1 of 2 green, the geometry test still failing (to finish); sat-page-order, sat-beta-notice, v2-undefined-names, nested-helper-scope, i18n green. No screenshots yet.

Resumed in worktree wt-ba-ui on origin/dev (after the beta.191-197 batch): WIP patch applied cleanly (3-way). The remaining red test was the test itself: its brace matcher skips strings but not regex literals, and v2.js esc() has /"/g; esc is now restated in the test (same three replacements), drawPanelChart stays the real one. panel-chart-style.test.mjs: old code 2 FAIL, fix 2 OK. Neighbours OK: v2-undefined-names 2, nested-helper-scope 2, i18n-lang 6, i18n-no-hardcoded 3, sat-page-order 1, sat-beta-notice 1, graph-panels 7. Headless Chrome with a real WebSocket streaming synthetic boiler frames: Mission control strip viewBox 780x256, panels flame/modulation/temperatures, 4 paths, 0 exceptions (screenshot scratchpad bench/c1841-mission.png); SAT control loop viewBox 780x302, panels 'Room · Outside', 'Setpoint · Flow', 'PID' drawn (partial screenshot bench/c1841-sat.png; full light/dark screenshots wait for a free Chrome slot, 09's 1763 run holds it).
<!-- SECTION:NOTES:END -->
