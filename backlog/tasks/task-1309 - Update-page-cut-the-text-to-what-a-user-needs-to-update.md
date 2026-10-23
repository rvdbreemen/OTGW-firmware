---
id: TASK-1309
title: 'Update page: cut the text to what a user needs to update'
status: To Do
assignee: []
created_date: '2026-10-23T07:51:00Z'
labels:
  - webui
  - update
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 432000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:33 (#alpha-testing, screenshot): the 'OTGW firmware Flash utility' update page is far too verbose (intro, Kept/Starts fresh lists, a blue info block about heating control, auth and DHCP, then a status line). Keep it simple: show only what is essential to update, and do not describe in text what is obvious. Keep the safety-relevant facts reachable (keep the tab open, heating not controlled for 1-2 min on OT-Direct) but short or behind a details toggle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The page shows at most: a one-line purpose, the file chooser with a short hint, the Update button, the unit/version line and one short warning line; the rest moves into a collapsed details element or is removed (before/after screenshot)
- [ ] #2 The update flow still works end to end on the bench (safeboot handover, firmware + web files, Done) after the text change
<!-- AC:END -->
