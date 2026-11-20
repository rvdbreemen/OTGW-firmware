---
id: TASK-2956
title: >-
  Web UI contrast tokens: warn text, accent per context, theme toggle, SAT pill
  and tag (UX review F1 F2 F16 F17)
status: To Do
assignee: []
created_date: '2026-11-20T14:10:00Z'
labels:
  - ux
  - webui
  - a11y
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1090000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F1, F2, F16, F17). Measured AA failures (WCAG 1.4.3): warn text in light 1.77:1 (v2.css:669-670, --zone-warn #ff9800); light accent #0288a0 as text 4.17:1 on #fff, 3.86:1 on #f1f7f8, 2.81:1 on the #383838 header, and #fff on #0288a0 4.17:1 (ds-tokens.css:62); dark theme toggle #fff on #00bffe 2.12:1; sat-pill Off 2.68:1; SAT->BOILER tag 2.13:1 (background computed, pixel check first). One accent token cannot pass both on the dark header and on light surfaces, so the fix is per context. Candidate values with measured ratios are in the review. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: mock-contrast-light.txt, mock-contrast-dark.txt, sat-simple-1400-light.png, mock-sat-control-1400-light.png. Owner: otgw-webui; design questions to otgw-ux-designer.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every text that uses the warn colour (all 6 color:var(--zone-warn) rules) measures >= 4.5:1 against its rendered background in light and dark
- [ ] #2 Accent text on a light surface (links, active SAT view tab and sub-label) measures >= 4.5:1 on #fff and on #f1f7f8
- [ ] #3 The active main-nav item on the dark header measures >= 4.5:1 in light and in dark
- [ ] #4 Text on an accent fill (active theme button, both themes) measures >= 4.5:1
- [ ] #5 SAT Off pill and SAT->BOILER tag measure >= 4.5:1 in light and dark, checked by pixel sampling on a screenshot
- [ ] #6 mock-contrast.mjs reports 0 FAIL lines in light and dark on Home, SAT (3 views) and Settings, and the ADR-091 drift gate is green
<!-- AC:END -->
