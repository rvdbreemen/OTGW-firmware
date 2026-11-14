---
id: TASK-1914
title: >-
  Web UI Monitor graph: reconnect marks look like restarts; add a legend/tooltip
  and a real reboot mark
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T23:18:00Z'
updated_date: '2026-11-14T22:24:00Z'
labels:
  - webui
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 980000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D read gruijter's beta.218 graph export (13 Nov, #alpha-testing 1570893817446539265) as 4 restarts; the log shows 3 (04:00:54, 08:38:18, 11:59:07; answered 1570896278173262018). The green/red dashed lines in Monitor > Graph are WebSocket marks: v2.js pushes {up:1} on reconnect and {up:0} on loss (v2.js:3206-3207) and renderGraphMarks draws them green/red with no legend or tooltip (v2.js:3229-3238). A sleeping tab or a WiFi hiccup gives the same green line as a restart. Proposal: (1) a legend entry and a tooltip "live connection lost / back (not a restart)"; (2) bonus: a distinct reboot mark derived from device/info uptime or a boot counter dropping, so a real restart is visible as such.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The graph shows a legend entry and a tooltip that name the dashed marks as live connection lost/back, not restarts (web UI test on the real v2.js files, red on the old files)
- [x] #2 A device reboot (uptime reset or boot counter change) is drawn as its own mark, distinct from a reconnect (web UI test with a simulated uptime reset)
- [x] #3 Screenshot before/after of Monitor > Graph
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (92, lane wt-92-webui). v2.js: each mark line gets an SVG <title> tooltip and a legend line #gMarkLegend (v2.html) under the series legend names the kinds on screen: 'live link lost' / 'live link back' (dashed red/green, tooltip '... Not a restart.') and 'gateway restarted' (solid amber #d29922). Restart detection: noteWsMark() reads device/info (withDeviceInfo, 8 s cache, so the startup snapshot on the first open and a fresh read after an outage) on the first open and on each return; noteDeviceBoot() keeps the boot counter as baseline and adds a restart mark when it went up (a lower count = counter reset = new baseline). Strings EN + NL/DE/EL (EL: my translation, consistent with el.json's 'ζωντανή σύνδεση με το gateway'; worth a native check). Tests one file at a time: new graph-mark-kinds.test.mjs 4/4 (real helpers cut from v2.js; on the old v2.js extraction fails: 0 pass 1 fail); graph-panels 7/7 (harness updated: device/info read once on the first open and once on the return, not on a drop; graphMarkKind now in graphMarkLines); i18n-lang 6/6; graph-break 8/8; panel-chart-style 2/2; graph-time-axis-and-png-export 11/11; evaluate --quick 0 FAIL. Screenshots (harness, fake WebSocket: open, drop, ~10 s outage, back; device/info boot count 7 -> 8): comms scratchpad t1914/old-graph-marks.png (two dashed lines, no legend) and fix-graph-marks.png (three lines, legend 'live link lost / live link back / gateway restarted').
<!-- SECTION:NOTES:END -->
