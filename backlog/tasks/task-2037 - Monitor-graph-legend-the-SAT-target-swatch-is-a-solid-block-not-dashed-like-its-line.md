---
id: TASK-2037
title: >-
  Monitor graph legend: the SAT target swatch is a solid block, not dashed like
  its line
status: To Do
assignee: []
created_date: '2026-11-18T22:57:00Z'
labels:
  - webui
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1045000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw-bench-tester observation on the TASK-2018 bench (.63, beta.378+1fcc894, 2026-11-18): the Monitor graph draws the SAT target series dashed (5 4, #e0c341), but its legend entry shows a solid colour block, so the legend does not say which line is dashed. Not a finding against TASK-2018 (its ACs hold); a small polish item.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The legend swatch of a dashed series (SAT target) shows the dash pattern of its line
- [ ] #2 Web UI test on the real v2.js: red on the code before, green after
<!-- AC:END -->
