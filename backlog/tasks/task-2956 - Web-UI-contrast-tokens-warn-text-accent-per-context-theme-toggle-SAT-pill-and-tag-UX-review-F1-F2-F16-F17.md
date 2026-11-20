---
id: TASK-2956
title: >-
  Web UI contrast tokens: warn text, accent per context, theme toggle, SAT pill
  and tag (UX review F1 F2 F16 F17)
status: To Do
assignee: []
created_date: '2026-11-20T14:10:00Z'
updated_date: '2026-11-20T15:02:00Z'
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
- [ ] #1 The tokens of spec T1 section 3 exist with the given values: --accent light #016f83, new --nav-accent, --on-accent, --hot-fill, --on-hot, --pill-off, --hot-text, and --status-warn-text light #8a4b00 used by the 6 warn-text rules (or, if the maintainer picks the alternative, --accent-strong on every text and text-bearing fill rule)
- [ ] #2 Every row of spec T1 section 2 measures at least its Proposal ratio, checked with mock-contrast.mjs in light and dark and by pixel sampling for the composite backgrounds (D1, D2b, D3, D4, F1, F2); 0 FAIL lines in both themes on Home, SAT (3 views) and Settings
- [ ] #3 The SAT pill is checked in heating, idle and off in both themes (heating needs a status fixture with a firing boiler_status)
- [ ] #4 v2.css:68 (.lang-menu li a.active color #0d2026) is replaced by --on-accent with no visual change
- [ ] #5 python evaluate.py --quick shows no new FAIL (ADR-091 class drift included) and the full tests/webui suite passes, sat-chart-actual-flow.test.mjs included
- [ ] #6 Before/after screenshots of Home, SAT Control and Settings in light and dark show only the intended colour changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T1-contrast-tokens.md (token table per context with measured ratios, ADR-091 consequence). New beyond the review: SAT pill Heating fails in both themes (3.10 light, 2.83 dark), dark tag 3.91. Correction: the light tag measures 2.59, not 2.13 (color-mix background misread by the scan). ADR-091 gate checks class drift only, not token values. Open for the maintainer via the coordinator: change light --accent (recommended) or add --accent-strong.
<!-- SECTION:NOTES:END -->
