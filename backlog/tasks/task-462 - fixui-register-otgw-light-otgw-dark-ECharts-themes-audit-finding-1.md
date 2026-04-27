---
id: TASK-462
title: 'fix(ui): register otgw-light/otgw-dark ECharts themes (audit finding 1)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-27 23:34'
labels: []
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
CRITICAL audit finding: sat.js calls echarts.init(c, 'otgw-light') / 'otgw-dark', but echarts-theme.js never calls echarts.registerTheme(). The names don't resolve, ECharts falls back to default colors, and the design tokens defined in otgwChartTheme() are never applied to the SAT chart or graph.js charts. Fix: in echarts-theme.js add registerOtgwThemes() that calls echarts.registerTheme('otgw-light', otgwChartTheme()) and same for 'otgw-dark'. Call once at load. In index.js theme:changed handler, call window.registerOtgwThemes() BEFORE SAT.setTheme()/OTGraph.setTheme() so the freshly-read CSS vars get registered before charts re-init.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 echarts-theme.js exposes registerOtgwThemes() and calls echarts.registerTheme for both otgw-light and otgw-dark
- [ ] #2 Initial registration runs once on script load (DOMContentLoaded or immediate)
- [ ] #3 index.js theme:changed handler calls window.registerOtgwThemes() before SAT.setTheme/OTGraph.setTheme to refresh registered theme with current CSS vars
- [ ] #4 SAT chart axis/grid/text colors track design tokens visibly
- [ ] #5 Build succeeds, 0 warnings
<!-- AC:END -->
