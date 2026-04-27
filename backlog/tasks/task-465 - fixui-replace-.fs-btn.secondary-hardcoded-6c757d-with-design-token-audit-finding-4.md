---
id: TASK-465
title: >-
  fix(ui): replace .fs-btn.secondary hardcoded #6c757d with design token (audit
  finding 4)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-27 23:35'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
components.css line 642: .fs-btn.secondary { background: #6c757d; } uses a hardcoded Bootstrap-grey. The other .fs-btn variants (.wifi, .danger) correctly use var(--status-warn) / var(--status-danger). Fix: use a design token. Best candidate is var(--bg-muted) or var(--fg-3) for a neutral grey that adapts in light/dark.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 components.css .fs-btn.secondary uses a var() reference instead of #6c757d
- [ ] #2 Token chosen produces acceptable contrast in both light and dark modes
- [ ] #3 Build clean
<!-- AC:END -->
