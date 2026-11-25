---
id: TASK-3051
title: 'Web UI 390 px: Advanced PIC firmware table first column truncated'
status: To Do
assignee: []
created_date: '2026-11-25T10:04:00Z'
labels:
  - webui
  - ux
milestone: 2.0.0
dependencies: []
ordinal: 1154000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen in the bench screenshot S12 (OTGW-field-data/bench-63/2026-11-25_beta-candidate/screenshots/S12-light-390.png): at 390 px the file table on Advanced > PIC firmware cuts the first column (FIRMWARE NAME) to one letter plus an ellipsis (d..., g..., i...) so the files cannot be told apart. Not a beta blocker; existing behaviour.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 At 390 px the PIC firmware file names are readable (wrap, scroll or stack) without hiding the version and the action buttons
- [ ] #2 Browser test at 390 px; screenshot light and dark
<!-- AC:END -->
