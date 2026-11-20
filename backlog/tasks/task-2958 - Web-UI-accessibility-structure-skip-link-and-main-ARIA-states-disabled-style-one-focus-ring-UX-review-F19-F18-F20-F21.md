---
id: TASK-2958
title: >-
  Web UI accessibility structure: skip link and main, ARIA states, disabled
  style, one focus ring (UX review F19 F18 F20 F21)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-20T14:22:00Z'
updated_date: '2026-11-20T17:11:00Z'
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
- [x] #1 The first Tab on any page focuses a visible 'Skip to content' link; Enter moves focus into <main id=main>; the Tab-walk script shows it as stop 1
- [x] #2 Exactly one <h1> (product name), one <main>, the page switcher is <nav aria-label=Main>, the Settings rail nav is labelled 'Settings sections', and each .page starts with a visually hidden <h2> with the page name
- [x] #3 After every page or view change exactly one .seg has aria-current=page and exactly one button per group (SAT views, Monitor tabs, Advanced tabs) has aria-current=true; a browser test checks all four groups
- [x] #4 #satEnable's accessible name is 'SAT' in both states and #satEnableLbl is aria-hidden
- [x] #5 With SAT off every disabled button (dial -/+, presets, Detect location) renders with opacity .5 and cursor not-allowed via one shared button:disabled rule, and the dial card shows the off hint; screenshots light and dark
- [x] #6 One :focus-visible rule with --focus-ring #1e88e5 (>= 3:1 on the header, light and dark pages, per spec T3 section 4) covers buttons, links, summaries and inputs; the Tab walk shows outline-style solid on every stop; the four old ring rules are gone; ADR-091 drift gate green
- [x] #7 New EN keys per spec T3; NL and DE through the i18n sync; tests/webui/i18n-*.test.mjs pass
- [x] #8 Before/after screenshots of Home, SAT and Settings in both themes show no visual change other than the disabled style, the off hint, the skip link on focus and the focus ring colour
- [x] #9 Every Settings rail item is a <button type=button> (today a <div> with a click handler, v2.js:4697-4701): the Tab walk reaches each category, Enter and Space open it, the active one has aria-current=true, no visual change; a browser test opens a category with the keyboard (WCAG 2.1.1 level A; spec T5 section 2.1)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T3-a11y-structure.md. ACs replaced by the spec's acceptance list. Change from the review: aria-current on view buttons instead of the ARIA tabs pattern (tabs need roving tabindex + arrow keys; without them role=tab misleads). New finding while specifying: the main nav is a <div class=switcher>; the only <nav> landmark is the Settings rail.

2026-11-20: priority raised to high and the rail-item part added by coordinator decision (moved from TASK-2960): keyboard users cannot reach any Settings category today.

2026-11-20 otgw-webui claimed; builds spec T3 sections 1-4 plus spec T5 section 2.1 (rail items as <button>).

2026-11-20 otgw-webui implementation (spec T3 sections 1-4, spec T5 section 2.1). v2.html: skip link 'Skip to content' as first element (href #main); header name is <h1 class=big>; page switcher <nav class=switcher aria-label=Main> (Home carries aria-current=page from the start); content container <main class=wrap id=main tabindex=-1>; each .page starts with <h2 class=sr-only> from the nav keys; Settings rail nav aria-label 'Settings sections'; #satEnable aria-label 'SAT', #satEnableLbl aria-hidden; dial off hint #satDialOffHint under the -/+ buttons (not in the collapsible #satMsgs panel, which is hidden unless a message is active). v2.css: one :focus-visible{outline:2px solid var(--focus-ring);outline-offset:2px}; the four accent ring rules lost their colour (offsets kept: lang menu items -2px, group summaries -2px, .sinfo 1px; the lang summary rule removed); .sr-only, .skip-link(:focus), main.wrap:focus{outline:none}; button:disabled{opacity:.5;cursor:not-allowed;box-shadow:none;transform:none}; .headerrow .big keeps the span look as an h1; .rail-item resets the button look (no visual change). ds-tokens.css: --focus-ring #1e88e5. v2.js: setAriaCurrent() in showPage (page), showTab, showAdvTab, satShowView (true); rail items created as <button type=button>, the active one gets aria-current=true; applySatLock() shows/hides #satDialOffHint. New keys core.skip_to_content, core.nav_main, core.nav_settings_sections, core.sat_switch_name, tools.sat_dial_off_hint in I18N_EN and nl/de/el.

Evidence. New test tests/webui/a11y-structure.test.mjs (real assets, headless Chrome, keyboard via CDP Input.dispatchKeyEvent, SAT off): red on origin/dev 8/8 fail for the right reasons (0 h1; first Tab lands on the connection button; focus ring rgb(0,191,254) on the lang summary; no aria-current; SAT switch unnamed; 9 disabled buttons at opacity 1 cursor pointer, the review's satDown/satUp/6 presets/satDetectLoc; rail items are DIVs); green 8/8 on the fix (first Tab = visible skip link, Enter -> MAIN#main; ring 2px solid rgb(30,136,229); aria-current one per group after page/view changes; Enter and Space open rail categories). settings-help.test.mjs updated: the ring now comes from the global :focus-visible rule (was a per-.sinfo outline), 29/29. Full tests/webui sweep (92 files, one at a time, timeout 180): 430 pass, 1 fail = that settings-help assertion, fixed and green since. evaluate.py --quick 88 passed, 0 warnings, 0 failed (design-system drift gate clean). i18n audit nl/de/el 0 missing 0 orphan. Screenshots before/after Home, SAT, Settings in light and dark plus the first Tab: OTGW-field-data/webui-screens/2026-11-20_task2958-a11y/{before,after}/; pixel diff: Home and Settings differ only at the clock (x 1099-1142, y 25-36) plus 104 px on Home light, SAT differs by the dimmed -/+ and presets and the off hint (card grows by one line). Not verified: a real screen reader (NVDA/VoiceOver); bench on a unit after the build.

2026-11-20 bench by otgw-bench-tester on .63, beta.404+9604698: PASS 8/8 (evidence OTGW-field-data/bench-63/2026-11-20_task2958-a11y/). Finding (not a blocker, coordinator decision): on the first open of a page the statically active tab carries class=active but no aria-current (Monitor Log v2.html:492, Advanced PIC :825, SAT view Thermostat :350); after every click it is correct, so AC3 ('after every page or view change exactly one') holds but the initial state does not. Follow-up as beta.405 after the 20:00 beta: aria-current=true on those three in the HTML plus an initial-state assertion in a11y-structure.test.mjs, own build and short bench.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Spec T3 sections 1-4 and spec T5 section 2.1 built in v2.html/v2.js/v2.css/ds-tokens.css and the nl/de/el packs. AC1 skip link first Tab, Enter -> main#main; AC2 one h1, one main, nav aria-label Main, labelled rail, hidden h2 per page; AC3 aria-current per group after page and view changes (initial state of the three statically active tabs: follow-up, see notes); AC4 SAT switch named SAT, state text aria-hidden; AC5 button:disabled opacity .5 / not-allowed, dial off hint; AC6 one :focus-visible ring #1e88e5, old ring colours gone, drift gate green; AC7 new keys in en/nl/de/el, i18n audit 0 missing; AC8 before/after screenshots Home/SAT/Settings light+dark in OTGW-field-data/webui-screens/2026-11-20_task2958-a11y/; AC9 rail items are buttons, Enter/Space open them. Tests: a11y-structure red 8/8 on origin/dev, green 8/8; full tests/webui sweep 430 pass (settings-help assertion updated, 29/29); evaluate 88/0/0. Bench .63 beta.404+9604698 PASS 8/8. Not verified: a real screen reader.
<!-- SECTION:FINAL_SUMMARY:END -->
