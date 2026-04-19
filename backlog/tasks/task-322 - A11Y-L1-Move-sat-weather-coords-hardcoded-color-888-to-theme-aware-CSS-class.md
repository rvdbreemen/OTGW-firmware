---
id: TASK-322
title: '[A11Y-L1] Move sat-weather-coords hardcoded color 888 to theme-aware CSS class'
status: To Do
assignee: []
created_date: '2026-04-18 19:25'
labels:
  - accessibility
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index.html:444 inline style color:888 at 0.85em (~11.9px). Against white this is 3.54:1, fails WCAG 1.4.3. Inline style also not theme-aware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Inline style removed; value uses a new CSS class
- [ ] #2 Per-theme color meets 4.5:1 on both light and dark themes
- [ ] #3 Coordinates still visually de-emphasized (secondary text)
<!-- AC:END -->
