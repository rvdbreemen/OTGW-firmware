---
id: TASK-784
title: Fix crowded tooltip in message support field
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 15:55'
updated_date: '2026-05-31 15:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The UI currently shows overlapping floating text when a long boiler unsupported-message list is displayed, making the field unreadable. Fix the presentation so long content remains readable without duplicate tooltip-style overlays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Long unsupported-message content is readable without overlapping duplicate floating windows
- [ ] #2 Any expanded/tooltip behavior appears only through a deliberate hover or focus interaction
- [ ] #3 The fix is scoped to the affected UI presentation without changing boiler message data
<!-- AC:END -->
