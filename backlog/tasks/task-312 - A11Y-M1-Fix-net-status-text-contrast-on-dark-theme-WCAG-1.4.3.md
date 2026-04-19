---
id: TASK-312
title: '[A11Y-M1] Fix net-status-text contrast on dark theme (WCAG 1.4.3)'
status: To Do
assignee: []
created_date: '2026-04-18 19:22'
labels:
  - accessibility
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index_dark.css:352 and index.css:354 use opacity 0.7 at 11px. Effective ~4.0:1 contrast, fails AA (needs 4.5:1).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Opacity removed; explicit per-theme color used instead
- [ ] #2 Measured contrast at least 4.5:1 against header background on both themes
- [ ] #3 Existing visual appearance preserved within a reasonable tolerance
<!-- AC:END -->
