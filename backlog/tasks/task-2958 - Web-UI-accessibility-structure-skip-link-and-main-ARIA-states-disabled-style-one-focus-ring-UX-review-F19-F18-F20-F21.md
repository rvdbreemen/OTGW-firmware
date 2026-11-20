---
id: TASK-2958
title: >-
  Web UI accessibility structure: skip link and main, ARIA states, disabled
  style, one focus ring (UX review F19 F18 F20 F21)
status: To Do
assignee: []
created_date: '2026-11-20T14:22:00Z'
labels:
  - ux
  - webui
  - a11y
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1092000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F19, F18, F20, F21). F19 (WCAG 2.4.1 A): no skip link, no <main>, no <h1>; 10 header stops before content. F18 (WCAG 4.1.2): main nav has no aria-current, SAT view tabs have no role=tab/aria-selected, the SAT switch is named 'Disabled' when off (double negative). F20: with SAT off, 9 buttons (dial lower/raise, 6 presets, Detect location) are disabled but render at opacity 1 with cursor:pointer; only .fs-btn:disabled has a style (v2.css:751). F21: browser default focus ring on most controls, designed 2 px accent ring on 4 (v2.css:54, 66, 449, 496); focus is visible, so this is consistency. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: mock-kbd-home-1400-light.txt, mock-kbd-sat-control-1400-light.txt, mock-a11y-sat-control.txt, mock-disabled-sat-control.txt, mock-focus-nav-light.png, mock-focus-nav-dark.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The first Tab on any page focuses a visible 'Skip to content' link that moves focus into <main>; each page has exactly one <main> and one <h1>
- [ ] #2 The active main-nav button has aria-current=page; the active SAT view tab has aria-selected=true and the others false
- [ ] #3 The SAT switch's accessible name is 'SAT' in both states
- [ ] #4 Every disabled button renders with one shared disabled style and cursor:not-allowed; with SAT off the dial card shows a one-line hint how to enable SAT
- [ ] #5 One :focus-visible rule covers buttons, links, summaries and inputs, in a colour >= 3:1 against both the header and the page
- [ ] #6 A browser test reads the ARIA attributes after a page and view change, and the Tab-walk script shows the skip link first and the same ring style on every stop
<!-- AC:END -->
