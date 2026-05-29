---
id: TASK-748
title: >-
  fix(webui): graph legend/title unreadable on dark theme (regression of
  TASK-568)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 07:54'
updated_date: '2026-05-29 07:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
graph.js init() derived currentTheme from localStorage('theme'), the wrong key — the canonical key driving body.dark and the design tokens is 'otgw-theme'. On a fresh load with a saved dark theme, currentTheme stayed 'light' while the body was dark, so ECharts init resolved to the light palette/fallback (#4a4d50 dark grey) against the dark surface -> legend + panel-title text unreadable. Toggle path (theme:changed -> OTGraph.setTheme) already worked; only fresh-load-with-saved-dark was broken. Fix: derive currentTheme from document.body.classList.contains('dark'); switch legend+title textStyle from --fg-2 (#e0e0e0 grey) to --fg-1 (dark=#ffffff white) per user request, fallback aligned. Regression of TASK-568.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 graph.js init() reads currentTheme from document.body 'dark' class, not localStorage key
- [ ] #2 legend + panel-title text uses --fg-1 (white in dark mode); fallback matches
- [ ] #3 Live theme toggle still updates the chart (no page reload needed)
- [ ] #4 node --check graph.js passes; filesystem build green (python build.py)
<!-- AC:END -->
