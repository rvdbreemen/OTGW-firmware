---
id: TASK-1914
title: >-
  Web UI Monitor graph: reconnect marks look like restarts; add a legend/tooltip
  and a real reboot mark
status: To Do
assignee: []
created_date: '2026-11-13T23:18:00Z'
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
- [ ] #1 The graph shows a legend entry and a tooltip that name the dashed marks as live connection lost/back, not restarts (web UI test on the real v2.js files, red on the old files)
- [ ] #2 A device reboot (uptime reset or boot counter change) is drawn as its own mark, distinct from a reconnect (web UI test with a simulated uptime reset)
- [ ] #3 Screenshot before/after of Monitor > Graph
<!-- AC:END -->
