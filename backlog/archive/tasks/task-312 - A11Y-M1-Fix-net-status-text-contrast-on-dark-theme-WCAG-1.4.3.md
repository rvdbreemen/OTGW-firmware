---
id: TASK-312
title: '[A11Y-M1] Fix net-status-text contrast on dark theme (WCAG 1.4.3)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:22'
updated_date: '2026-04-19 06:20'
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
- [x] #1 Opacity removed; explicit per-theme color used instead
- [x] #2 Measured contrast at least 4.5:1 against header background on both themes
- [x] #3 Existing visual appearance preserved within a reasonable tolerance
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.css and index_dark.css: replaced opacity:0.7 on .net-status-text with explicit per-theme colors (#555 light, #c0c0c0 dark), both at ~5:1 or higher against the header background. Visual de-emphasis preserved by using a muted color rather than the opacity hack that broke WCAG AA.
<!-- SECTION:FINAL_SUMMARY:END -->
