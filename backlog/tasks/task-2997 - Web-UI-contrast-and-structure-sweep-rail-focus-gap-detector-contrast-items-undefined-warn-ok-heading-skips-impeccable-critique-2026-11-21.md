---
id: TASK-2997
title: >-
  Web UI contrast and structure sweep: rail focus gap, detector contrast items,
  undefined --warn/--ok, heading skips (impeccable critique 2026-11-21)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T07:58:00Z'
updated_date: '2026-11-25T12:30:00Z'
labels:
  - webui
  - ux
  - a11y
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1123000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the impeccable critique of 2026-11-21 by otgw-ux-designer (dev web UI after TASK-2956 and TASK-2991, origin/dev a31f97664). (1) Rail focus ring: outline-offset 2px with a 2px gap between rail items (v2.css:41, :438) overlaps a neighbour's active tint (about 2.94:1 in dark, not visually verified); use gap 4px or outline-offset:-2px as on .seg (v2.css:47). (2) Detector-only contrast fails in the static v2.html (impeccable detect, positions not traceable, line 0): #4caf50 on #f1f7f8 2.6:1, #8a8f92 on #f1f7f8 3.0:1, #f44336 on #fff 3.7:1, white on #d17d00 3.2:1, #258395 on #f1f7f8 4.1:1; locate each element first. (3) --warn and --ok are undefined, so the MQTT onboarding step falls back to #e0a300 / #27c469 on #fff (about 2.2-2.3:1). (4) h2 followed by h4 under SAT, Settings, Advanced and Diagnose (skipped heading level). The wizard #04222c-on-accent buttons and the SVG chip at v2.html:727 are NOT in this task (separate high task).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The rail focus ring no longer overlaps a neighbouring item (gap or inset offset), checked in dark and light with a screenshot via otgw-bench-tester
- [x] #2 Each detector-only contrast pair is traced to its element and reaches >= 4.5:1 for text (3:1 for large text and UI components), re-run with impeccable detect showing none of these pairs
- [x] #3 --warn and --ok are defined or the MQTT step uses existing tokens, with its texts >= 4.5:1 in both themes
- [x] #4 No skipped heading levels on SAT, Settings, Advanced and Diagnose (impeccable detect skipped-heading gives 0)
- [x] #5 python evaluate.py --quick shows no new FAIL and tests/webui pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 (lane/ba-2997 from origin/dev 5bcd5b54c). AC2: the 8 detector findings (impeccable detect on v2.html, 45 anti-patterns before) traced to elements with an in-page scan that composites backgrounds and opacity, all pages forced visible: white on #d17d00 = .hdr-sim and .fw-sim (SIMULATION, zone-warn 82% mix -> 60%, white 5.45 light / 5.01 dark); #4caf50 on #f1f7f8 = .mc h2 b (// MISSION CONTROL); #4caf50 on #ddeee4 = .crashlog-ok; #f44336 on #fff = .fs-btn.danger (Reset Wireless); #258395 and #8a8f92 on #f1f7f8 = .satview-btn small (opacity .7 and .85 removed); the two placeholders #566069 on #0a0e14 = .cmdbar-in::placeholder -> #8b98a5 (6.57). New tokens --status-ok-text (#1b5e20 light, #81c784 dark) and --status-error-text (#c62828 light, #ff8a80 dark) in ds-tokens.css. Also fixed because the same scan showed them real: .toast white on the ok green (2.78) now on --status-ok-dark (5.13 / 7.87), .fw-conn badge white on a tint (1.77) now status text on a panel tint. Result: impeccable detect low-contrast 8 -> 0, 45 -> 33 findings in total (evidence-TASK-2997/imp_before.txt, imp_after.txt). Rows G1-K1 added to tests/webui/contrast-tokens.test.mjs (all >= 4.5). AC3: the wizard (MQTT onboarding step) used undefined --ok and --warn: texts now var(--status-ok-text) and var(--status-warn-text), the welcome circle and dot var(--status-ok). AC4: 24 h4 in v2.html and 3 createElement('h4') became h3, the 6 CSS selectors with them; impeccable skipped-heading 4 -> 0; tests that selected h4 updated (about-cards-typography, otd-hidden-in-pic-mode, sat-display-states, sat-page-order, sat-pwm-phases). AC1 (rail focus ring): .rail-item:focus-visible{outline-offset:-2px}, the ring sits inside the item; asserted as a CSS rule, the dark and light screenshot via otgw-bench-tester is NOT done, so AC1 stays unticked. AC5: evaluate.py --quick 88 pass 0 fail; tests/webui related: 32 files run, all pass after the about-cards fix. Evidence: tests/webui/heading-levels-and-rail-focus-2997.test.mjs red 0/3 on origin/dev data (h2 then h4 at 4 places, rail offset, --ok/--warn in v2.js), green 3/3 on the fix; contrast rows MISSING the new tokens on old data. Not done: other real contrast gaps the scan showed and the detector does not count (connection legend glyphs white on status colours 2.2-3.7, Home chip values #ff5a3c/#2f8fe8 on the schematic card ~3, language code in the menu 3.7-3.8, dropdown caret 2.7) and the tiny-text/undersized-ui-text findings (28): separate follow-up if wanted. EL: no new strings here.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Detector contrast pairs traced and fixed (new tokens --status-ok-text/--status-error-text, SIMULATION badge, toast, connection badge, command-bar placeholder), wizard uses defined tokens, h4 under h2 became h3, rail focus ring inset. Evidence: tests/webui heading-levels-and-rail-focus-2997 and contrast-tokens rows G1-K1 red on origin/dev data and green on the fix; impeccable detect on v2.html low-contrast 8 to 0, skipped-heading 4 to 0; evaluate --quick 88/0; full webui suite 108 files green (sat-page-order flaked once under parallel load, 4x green alone). Bench (otgw-bench-tester, .63, build beta.431 / 3d0df2d, OTGW-field-data/bench-63/2026-11-25_webui-431/): S20/S21 rail focus ring 2px inside the item, neighbour tint not overlapped, Tab order and Enter ok (AC1); S27 crash-log ok-line contrast about 6.5 light / 7.2 dark (bench estimate); S28 toast Settings saved. Not verified: S26 SIMULATION badge on the bench (no simulator), real touch device, screen reader. Published in zip beta.431 (3d0df2d), pushed 3d0df2db0. Remaining gaps are TASK-3049 (2.1.0).
<!-- SECTION:FINAL_SUMMARY:END -->
