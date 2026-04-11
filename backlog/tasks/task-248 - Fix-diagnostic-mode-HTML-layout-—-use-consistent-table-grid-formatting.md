---
id: TASK-248
title: Fix diagnostic mode HTML layout — use consistent table/grid formatting
status: To Do
assignee: []
created_date: '2026-04-11 09:26'
labels:
  - frontend
  - ui
  - diagnostic
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The diagnostic mode pages contain output areas that are not formatted as table/grid, making them hard to read. Specifically: (1) #deviceinfoPage has no display:table parent wrapper so the display:table-row/.devinfocolumn1/.devinfocolumn2 rows do not render as a table. (2) renderCrashLogInfo() in index.js uses plain div+pre elements instead of a structured grid/table. (3) All diagnostic sections should use consistent styling. Fix applies to index.css, index_dark.css, and index.js.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Add display:table (or CSS grid) to #deviceinfoPage in index.css so existing table-row/table-cell rows render correctly
- [ ] #2 Fix renderCrashLogInfo() in index.js to present crash data in a structured table/grid instead of plain <pre> elements
- [ ] #3 All diagnostic sections use consistent table/grid formatting and styling
- [ ] #4 Changes reflected in index_dark.css as well
<!-- AC:END -->
