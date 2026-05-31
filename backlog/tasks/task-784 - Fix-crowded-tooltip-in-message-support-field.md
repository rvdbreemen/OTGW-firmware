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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the UI code that renders the boiler unsupported-message field and its tooltip/overflow handling.
2. Change the presentation so long text is readable without duplicate overlays, preferring one explicit hover/focus affordance if needed.
3. Add or update focused tests/checks if this UI has static validators, then run the relevant validation commands.
4. Update TASK-784 acceptance criteria, notes, and final summary after verification.
<!-- SECTION:PLAN:END -->
