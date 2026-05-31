---
id: TASK-783
title: Fix small viewport settings field wrapping
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 15:08'
updated_date: '2026-05-31 15:08'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On narrow screens, settings rows currently keep label text and input/control on the same line in places where the responsive layout should stack them. Scope the fix to small viewport rendering only so desktop layout remains unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 At the small viewport breakpoint, label text renders on its own line before the associated input/control.
- [ ] #2 Each stacked setting keeps the input/control before the next card or row begins.
- [ ] #3 Desktop settings layout remains unchanged.
- [ ] #4 Validation covers the affected responsive CSS/markup path.
<!-- AC:END -->
