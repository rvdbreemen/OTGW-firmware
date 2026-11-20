---
id: TASK-2958
title: >-
  Web UI accessibility structure: skip link and main, ARIA states, disabled
  style, one focus ring (UX review F19 F18 F20 F21)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:22:00Z'
updated_date: '2026-11-20T16:00:00Z'
labels:
  - ux
  - webui
  - a11y
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1092000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F19, F18, F20, F21). F19 (WCAG 2.4.1 A): no skip link, no <main>, no <h1>; 10 header stops before content. F18 (WCAG 4.1.2): main nav has no aria-current, SAT view tabs have no role=tab/aria-selected, the SAT switch is named 'Disabled' when off (double negative). F20: with SAT off, 9 buttons (dial lower/raise, 6 presets, Detect location) are disabled but render at opacity 1 with cursor:pointer; only .fs-btn:disabled has a style (v2.css:751). F21: browser default focus ring on most controls, designed 2 px accent ring on 4 (v2.css:54, 66, 449, 496); focus is visible, so this is consistency. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: mock-kbd-home-1400-light.txt, mock-kbd-sat-control-1400-light.txt, mock-a11y-sat-control.txt, mock-disabled-sat-control.txt, mock-focus-nav-light.png, mock-focus-nav-dark.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The first Tab on any page focuses a visible 'Skip to content' link; Enter moves focus into <main id=main>; the Tab-walk script shows it as stop 1
- [ ] #2 Exactly one <h1> (product name), one <main>, the page switcher is <nav aria-label=Main>, the Settings rail nav is labelled 'Settings sections', and each .page starts with a visually hidden <h2> with the page name
- [ ] #3 After every page or view change exactly one .seg has aria-current=page and exactly one button per group (SAT views, Monitor tabs, Advanced tabs) has aria-current=true; a browser test checks all four groups
- [ ] #4 #satEnable's accessible name is 'SAT' in both states and #satEnableLbl is aria-hidden
- [ ] #5 With SAT off every disabled button (dial -/+, presets, Detect location) renders with opacity .5 and cursor not-allowed via one shared button:disabled rule, and the dial card shows the off hint; screenshots light and dark
- [ ] #6 One :focus-visible rule with --focus-ring #1e88e5 (>= 3:1 on the header, light and dark pages, per spec T3 section 4) covers buttons, links, summaries and inputs; the Tab walk shows outline-style solid on every stop; the four old ring rules are gone; ADR-091 drift gate green
- [ ] #7 New EN keys per spec T3; NL and DE through the i18n sync; tests/webui/i18n-*.test.mjs pass
- [ ] #8 Before/after screenshots of Home, SAT and Settings in both themes show no visual change other than the disabled style, the off hint, the skip link on focus and the focus ring colour
- [ ] #9 Every Settings rail item is a <button type=button> (today a <div> with a click handler, v2.js:4697-4701): the Tab walk reaches each category, Enter and Space open it, the active one has aria-current=true, no visual change; a browser test opens a category with the keyboard (WCAG 2.1.1 level A; spec T5 section 2.1)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T3-a11y-structure.md. ACs replaced by the spec's acceptance list. Change from the review: aria-current on view buttons instead of the ARIA tabs pattern (tabs need roving tabindex + arrow keys; without them role=tab misleads). New finding while specifying: the main nav is a <div class=switcher>; the only <nav> landmark is the Settings rail.

2026-11-20: priority raised to high and the rail-item part added by coordinator decision (moved from TASK-2960): keyboard users cannot reach any Settings category today.

2026-11-20 otgw-webui claimed; builds spec T3 sections 1-4 plus spec T5 section 2.1 (rail items as <button>).
<!-- SECTION:NOTES:END -->
