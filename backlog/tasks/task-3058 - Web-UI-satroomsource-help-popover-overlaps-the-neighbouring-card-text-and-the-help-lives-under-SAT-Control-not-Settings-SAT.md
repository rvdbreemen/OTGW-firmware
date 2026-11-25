---
id: TASK-3058
title: >-
  Web UI: satroomsource help popover overlaps the neighbouring card text, and
  the help lives under SAT Control, not Settings > SAT
status: To Do
assignee: []
created_date: '2026-11-25T12:27:00Z'
labels:
  - webui
  - ux
milestone: 2.1.0
dependencies: []
ordinal: 1158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench screenshot S22 (OTGW-field-data/bench-63/2026-11-25_webui-431/screenshots/S22-light-1400-help.png, build 3d0df2d): the (i) popover of Room temperature source (satroomsource) overlaps the text of the neighbouring card. The setting is reached at SAT page > Control > SAT settings > Indoor temperature; Settings > SAT only shows Open SAT settings. Not a beta blocker: the text is complete and not cut off (scrollHeight = clientHeight).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The help popover of a SAT setting never overlaps the text of the neighbouring card at 1400 and 390 px, light and dark
- [ ] #2 Browser test with the popover open and screenshot as evidence
<!-- AC:END -->
