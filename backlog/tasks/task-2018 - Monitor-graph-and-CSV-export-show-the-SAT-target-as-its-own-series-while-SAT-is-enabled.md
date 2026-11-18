---
id: TASK-2018
title: >-
  Monitor graph and CSV export: show the SAT target as its own series while SAT
  is enabled
status: To Do
assignee: []
created_date: '2026-11-18T09:30:00Z'
labels:
  - webui
  - sat
  - graph
dependencies: []
priority: medium
ordinal: 1034000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
92's analysis of indigo's graph CSVs (via coordinator, 2026-11-18): the Monitor graph and its CSV export plot roomSet = MsgID 16 TrSet (v2.js:2392, live model; graphHistoryStuff.ino:61 for the device ring), also while SAT is active. The setpoint that actually drives the boiler under SAT is the SAT target, which the graph never shows. Checked 2026-11-18 in wt-ba-2015: the 24 h/7 d device history ring (ADR-193, ADR-245) does NOT store the SAT target: GraphHistSlot (graphHistoryStuff.h:62-72) holds flow, ret, chSet, roomSet, room, outside, mod, flags, static_assert 14 bytes. Options: A) add an int16 satTarget column (slot 14 -> 16 bytes, ring RAM/PSRAM +14 %, ring format and /graph-history response change: needs an ADR amendment of ADR-193/245 and Robert's OK); B) live-only series in the browser from /v2/sat/status (no ring change, history panel shows it only for the open-tab span); C) write the SAT target into the roomSet column while SAT is active (no format change, but the column's meaning becomes mode-dependent). Do after TASK-2015 / beta.371.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The Monitor graph shows a separate SAT target series while SAT is enabled, with its own legend entry and translation in en/nl/de/el
- [ ] #2 The CSV export carries the SAT target as its own column
- [ ] #3 Decision on the history source (ring column vs live-only) recorded in the task; a ring format change goes through an ADR amending ADR-193/245
- [ ] #4 Web UI test drives the real v2.js: red on the old code, green on the fix; screenshot with synthetic data
<!-- AC:END -->
