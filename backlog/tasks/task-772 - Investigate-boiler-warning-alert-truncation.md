---
id: TASK-772
title: Investigate boiler warning alert truncation
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 08:37'
updated_date: '2026-05-31 08:40'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Investigate why the yellow boiler warning alert in the web UI truncates text like 'Boiler does not implement: ...' and report the root cause without changing UI code unless separately requested.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause is traced to the relevant markup, JavaScript, or CSS rule.
- [ ] #2 No source changes are made as part of this investigation unless explicitly requested.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the OT Support alert markup and CSS for the warning message shown as 'Boiler does not implement: ...'.
2. Identify the exact truncation rule and explain why the browser renders an ellipsis there.
3. Keep the investigation read-only unless the user explicitly asks for a UI change.
<!-- SECTION:PLAN:END -->
