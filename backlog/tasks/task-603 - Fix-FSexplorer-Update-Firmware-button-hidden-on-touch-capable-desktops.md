---
id: TASK-603
title: Fix FSexplorer Update Firmware button hidden on touch-capable desktops
status: In Progress
assignee:
  - '@copilot'
created_date: '2026-05-15 16:15'
updated_date: '2026-05-15 16:16'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Update Firmware button in FSexplorer is hidden when (pointer: coarse) and (hover: none), which incorrectly affects devices like Surface Pro. Make the button visible on desktop-class layouts while keeping mobile behavior explicit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 FSexplorer Update Firmware action remains visible on desktop-class viewports even when device reports coarse pointer
- [ ] #2 Touch-only mobile layouts still hide the update action when intended
- [ ] #3 FSexplorer light and dark theme styles stay consistent
<!-- AC:END -->
