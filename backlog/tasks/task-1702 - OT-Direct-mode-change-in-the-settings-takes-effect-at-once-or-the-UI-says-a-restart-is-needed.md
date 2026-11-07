---
id: TASK-1702
title: >-
  OT-Direct mode change in the settings takes effect at once or the UI says a
  restart is needed
status: To Do
assignee: []
created_date: '2026-11-07T17:05:00Z'
labels:
  - otdirect
  - webui
  - field-report
dependencies: []
priority: medium
ordinal: 815000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-07 14:01 UTC: after switching OT-Direct mode to Gateway the monitor page still showed MODE: MASTER; after a reboot it was in Gateway mode. Verify in code whether the mode change is applied live; then either apply it live or show that a restart is needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Code path for a live OT-Direct mode change verified and documented in the notes
- [ ] #2 After saving a new OT-Direct mode the monitor page shows the mode the bus actually runs in, or the settings page states that a restart is needed
- [ ] #3 Bench evidence on an OT-Direct unit
<!-- AC:END -->
