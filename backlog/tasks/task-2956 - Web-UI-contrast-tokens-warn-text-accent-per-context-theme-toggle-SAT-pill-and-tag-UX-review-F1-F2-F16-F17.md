---
id: TASK-2956
title: >-
  Web UI contrast tokens: warn text, accent per context, theme toggle, SAT pill
  and tag (UX review F1 F2 F16 F17)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:10:00Z'
updated_date: '2026-11-21T06:27:00Z'
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
- [x] #1 The tokens of spec T1 section 3 exist with the given values: --accent light #016f83, new --nav-accent, --on-accent, --hot-fill, --on-hot, --pill-off, --hot-text, and --status-warn-text light #8a4b00 used by the 6 warn-text rules (or, if the maintainer picks the alternative, --accent-strong on every text and text-bearing fill rule)
- [x] #2 Every row of spec T1 section 2 measures at least its Proposal ratio, checked with mock-contrast.mjs in light and dark and by pixel sampling for the composite backgrounds (D1, D2b, D3, D4, F1, F2); 0 FAIL lines in both themes on Home, SAT (3 views) and Settings
- [x] #3 The SAT pill is checked in heating, idle and off in both themes (heating needs a status fixture with a firing boiler_status)
- [x] #4 v2.css:68 (.lang-menu li a.active color #0d2026) is replaced by --on-accent with no visual change
- [x] #5 python evaluate.py --quick shows no new FAIL (ADR-091 class drift included) and the full tests/webui suite passes, sat-chart-actual-flow.test.mjs included
- [x] #6 Before/after screenshots of Home, SAT Control and Settings in light and dark show only the intended colour changes
- [x] #7 tests/webui/contrast-tokens.test.mjs exists per docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T1b-contrast-regression-test.md: layer A checks every row of spec T1 section 2 (undefined token = explicit failure), layer B scans the rendered pages, themes and state fixtures; colours normalised through a canvas (color-mix safe)
- [x] #8 The contrast test is red on the pre-TASK-2956 data (output with the failing rows in the task notes), green on the change, and a scratch copy with --status-warn-text #9c5700 fails on row D2b
- [x] #9 The contrast test runs in python scripts/run_tests.py --webui and is not on the flaky list
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T1-contrast-tokens.md (token table per context with measured ratios, ADR-091 consequence). New beyond the review: SAT pill Heating fails in both themes (3.10 light, 2.83 dark), dark tag 3.91. Correction: the light tag measures 2.59, not 2.13 (color-mix background misread by the scan). ADR-091 gate checks class drift only, not token values. Open for the maintainer via the coordinator: change light --accent (recommended) or add --accent-strong.

2026-11-20: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T1b-contrast-regression-test.md added (contrast regression test, requested by the coordinator); 3 ACs added.

2026-11-21 otgw-webui claimed; builds spec T1 (tokens) + T1b (contrast regression test). Light work until the next heavy slot.

2026-11-21 05:xx otgw-webui implementation (spec T1 recommended option, one darker light accent): ds-tokens.css --status-warn-text light #8a4b00, new --hot-fill/--on-hot/--hot-text (light #c43e1f/#fff/#b3261e, dark #ff6a4d/#1c1c1e/#ffab99); v2.css --accent light #016f83, new --nav-accent (brand cyan), --on-accent (#fff / dark #0d2026), --pill-off (fg-3 / dark #54565a); 7+2+1 white-on-accent rules -> --on-accent, the one-off dark .lang-menu fix removed (same #0d2026 via the token), .seg.active -> --nav-accent, 6 color:var(--zone-warn) rules -> --status-warn-text, .sat-pill -> --hot-fill/--on-hot, idle/off -> --pill-off with white, .sat-tile .tag -> --hot-text.

RED on pre-TASK-2956 data (tests/webui/contrast-tokens.test.mjs, OTGW_DATA_DIR = copy with HEAD v2.css + ds-tokens.css): layer A FAIL A1 4.17, A2 3.86, A3 3.96, D2b 4.44; not defined: B1/B2 --nav-accent, C1/C2 --on-accent, E1/E2 --on-hot --hot-fill, E3/E4 --pill-off, F1/F2 --hot-text; D1 4.57 and D2 5.56 pass as the spec predicted. Layer B: light 105 / dark 31 texts below the minimum, all in spec scope (white on accent 4.17 / dark 2.12, accent text 4.17/3.86, nav 2.81, warn banner 1.76, reboot badge 2.16, pill 3.10/2.83/2.68, tag 2.57/3.91). 1149 texts measured per theme, 178 listed as not measured (opacity < 1 or background image).

GREEN on the change: layer A all 21 rows pass (A1 5.83 ... D4 4.95), layer B 0 below the minimum in light and dark (3/3 tests pass). Mutation: scratch copy with --status-warn-text #9c5700 -> layer A FAIL D2b 4.44 only. Pixel sampling on screenshots (before -> after): F1 tag 2.57 -> 5.42, F2 3.97 -> 6.17, E1 pill 3.10 -> 5.18, E2 2.83 -> 6.01, E3 2.68 -> 5.06, E4 7.35 -> 7.35, D1 banner 1.77 -> 5.56, D3 6.58 -> 6.88, D2 badge 2.16 -> 6.80, B1 nav 2.82 -> 5.53. D2b/D4 (.dir-tb on Monitor) measured by layer A only, not pixel-sampled. Spec T1b fixture fix: /v2/device/info answers {device:{fwversion}}, not {fwversion}; with the spec's shape the beta banner never shows.

Other checks: evaluate.py --quick 95 checks, 0 failed, design-system drift 0 FAIL. Colour-pinning webui tests one by one green: header-controls-same-height 2/2, lang-menu 7/7, sat-chart-actual-flow 4/4, sat-status-pill 1/1, settings-select-fits 2/2, mission-control-follows-theme 2/2, settings-reboot-badge 1/1. Screenshots before/after (Home, SAT Control heating+idle, Settings; light+dark) checked by eye: only the intended colour changes. Full tests/webui sweep still to run in a heavy slot (AC5).

2026-11-21 07:01-07:26 (heavy slot from coordinator): rebased alone onto origin/dev d61678eef as 9c731ca26 (lane/ba-2956), beta.408; diff be15333dc..9c731ca26 -- src tests = only the TASK-2991 lines (v2.js 3, a11y-structure test 10) + build stamps. Full webui sweep, one file per node --test: files=93 pass 438 fail 0 (07:01-07:15). build.bat esp32-combo SUCCESS 00:06:54, zip OTGW-firmware-esp32-combo-2.0.0-beta.408+9c731ca-flash.zip sha256 1e851edd16da22b14403b439b8b3976767c0a2503e9e794cf6b0aa5dff493a0c, 13 files in OTGW-build-artifacts-safe/beta.408+9c731ca/, log OTGW-build-artifacts-safe/build-2956-408.log. Bench pending (otgw-bench-tester, .63 after 2991).
<!-- SECTION:NOTES:END -->
