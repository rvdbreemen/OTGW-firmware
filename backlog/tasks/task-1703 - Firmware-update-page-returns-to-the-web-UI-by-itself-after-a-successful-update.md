---
id: TASK-1703
title: Firmware update page returns to the web UI by itself after a successful update
status: To Do
assignee: []
created_date: '2026-11-07T17:06:00Z'
labels:
  - webui
  - field-report
dependencies: []
priority: low
ordinal: 816000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-07 14:21 UTC: after a successful update the user has to press a button to go back to the web UI; it should go there automatically.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a successful firmware or filesystem update the update page waits until the unit answers again and then opens the web UI without a click
- [ ] #2 On a failed update the page stays and shows the error
- [ ] #3 Bench evidence (screenshots or capture of the redirect)
<!-- AC:END -->
