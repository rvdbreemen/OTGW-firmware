---
id: TASK-1110
title: Highlight firmware table cells that actually changed after a web download
status: To Do
assignee: []
created_date: '2026-09-02 21:24'
labels: []
dependencies: []
ordinal: 205000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The refresh (update-from-web) button on the PIC firmware tab rewrites the version cell in place, and does not touch the size cell at all even though a download usually changes it. Nothing tells the user whether the click did anything: a file already at the latest version is a silent no-op that looks identical to a successful download. Mark every cell whose value actually changed with a green highlight that fades after about 15 seconds, so a real update is visible and a no-op stays quiet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The size cell is updated from the refreshed file list, not only the version cell
- [ ] #2 A cell whose value actually changed is highlighted green and returns to normal after about 15 seconds
- [ ] #3 A cell whose value did not change is not highlighted, so a no-op refresh stays visually quiet
- [ ] #4 The highlight is legible in both the light and the dark theme
- [ ] #5 Repeated clicks restart the highlight rather than leaving a stuck or double-scheduled timer
<!-- AC:END -->
