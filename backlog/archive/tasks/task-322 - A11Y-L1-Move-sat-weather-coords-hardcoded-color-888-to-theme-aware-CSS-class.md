---
id: TASK-322
title: '[A11Y-L1] Move sat-weather-coords hardcoded color 888 to theme-aware CSS class'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:25'
updated_date: '2026-04-19 06:21'
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
- [x] #1 Inline style removed; value uses a new CSS class
- [x] #2 Per-theme color meets 4.5:1 on both light and dark themes
- [x] #3 Coordinates still visually de-emphasized (secondary text)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index.html:444: replaced inline style='font-size:0.85em;color:#888' with class='sat-weather-coords'. New rule added to both index.css (#666) and index_dark.css (#999), both meet WCAG AA for secondary text against their respective theme backgrounds. Coordinates still visually de-emphasised.
<!-- SECTION:FINAL_SUMMARY:END -->
