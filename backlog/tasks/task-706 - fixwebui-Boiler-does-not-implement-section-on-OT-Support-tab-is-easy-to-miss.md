---
id: TASK-706
title: >-
  fix(webui): 'Boiler does not implement' section on OT Support tab is easy to
  miss
status: To Do
assignee: []
created_date: '2026-05-25 20:40'
labels:
  - bug
  - webui
  - ux
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans on #beta-testing (2026-05-25) via screenshot. The 'Boiler does not implement' list on the OT Support tab is rendered as a plain text line at the bottom of the table and is easy to overlook. Make it visually distinct — e.g. a styled card/banner below the table — so users notice which message IDs their boiler rejects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The 'Boiler does not implement' section on the OT Support tab is visually separated from the table (not inline footer text)
- [ ] #2 When the list is empty (boiler implements everything), nothing extra is rendered
- [ ] #3 Styling is consistent with existing WebUI conventions (no new CSS frameworks)
- [ ] #4 python build.py exits 0; python evaluate.py --quick shows no new failures
<!-- AC:END -->
