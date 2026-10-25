---
id: TASK-1398
title: 'v2 Settings: explain the orange REBOOT badge (looks clickable, no explanation)'
status: To Do
assignee: []
created_date: '2026-10-25T19:49:00Z'
labels:
  - needs-maintainer-ok
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 537000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563987220115890257): on Settings > Network an orange 'REBOOT' badge shows without explanation and looks like a button but is not clickable. It marks reboot-sensitive categories (v2.js:2446, :2469). Option: a tooltip/aria-label 'Changes here take effect after a reboot' and a non-button style.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Badge has a tooltip and does not look clickable (screenshot)
<!-- AC:END -->
