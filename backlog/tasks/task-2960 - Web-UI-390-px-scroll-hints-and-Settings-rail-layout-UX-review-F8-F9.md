---
id: TASK-2960
title: Web UI 390 px scroll hints and Settings rail layout (UX review F8 F9)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:30:00Z'
updated_date: '2026-11-25T06:06:00Z'
labels:
  - ux
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1094000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F8, F9). F8: at 390 px the main nav, the Settings rail and the Home chips are cut off with no sign they scroll (overflow-x:auto, v2.css:108, v2.css:535). F9: at 1400 px the Network rail item puts its icon above the label because of the REBOOT badge, long labels wrap, and the panel uses about 370 of 1000 px. Open question for otgw-webui: is the Settings rail SAT group (90 settings) the same set as 'SAT settings' at the bottom of SAT > Control (v2.html:444-445), one source rendered twice or two sets? Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: home-390-dark.png, settings-390-dark.png, mock-home-390-light.png, mock-settings-390-light.png, settings-1400-dark.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 At 390 px the 5 strips of spec T5 section 1 (main nav, SAT views, Monitor/Advanced tabs, Settings rail, Home chips) show the edge fade on the side with hidden content in both themes, the fade follows scrolling, and the active item is in view after a page or view change; before/after screenshots
- [x] #2 At 1400 px every rail item keeps icon and label on one line, long titles end in an ellipsis with the full title as title, Network shows the reboot dot instead of the text badge, and the 3 short rail titles are used
- [x] #3 A category with at most 2 cards renders in one column of up to 760 px when no search is active
- [x] #4 New classes have selectors, python evaluate.py --quick shows no new FAIL, new EN keys go through the i18n sync and tests/webui/i18n-*.test.mjs pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T5-mobile-strips-and-rail.md. ACs replaced. Open question answered: the Settings SAT group and SAT > Control settings are one source (setData, catFor==='sat'), two groupings. New finding: .rail-item is a div with a click handler, not keyboard reachable (WCAG 2.1.1 level A); the coordinator may move that part to TASK-2958.

2026-11-20: the rail-item <div> -> <button> + aria-current part (spec T5 section 2.1, WCAG 2.1.1) moved to TASK-2958 by coordinator decision; TASK-2960 keeps the layout only.

2026-11-25 (lane/ba-2960 from origin/dev 5bcd5b54c). AC1: .hscroll on the main nav, SAT views, both .subtabs rows (Monitor, Advanced), the Settings rail and the Home chips; syncHScroll() sets data-overflow end/both/start from scrollLeft/clientWidth/scrollWidth (1 px tolerance) on scroll, resize, ResizeObserver and a MutationObserver (childList, class, aria-current), masks in v2.css with the -webkit twin; the active item is scrolled sideways into view when it changes (smooth, instant under prefers-reduced-motion), without scrolling the page. AC2: rail item = .ri-ic + .ri-lbl (ellipsis, full title in title) + .ri-reboot dot (Network) + count; the full REBOOT badge stays in the category header and on the rows; short titles settings.cat.otgw/sat/s0.short (EN, NL, DE, EL; EL written by me, add to TASK-3047). AC3 (rail as a button) was done in TASK-2958 and is untouched, so tick it as already done there. AC4: set-cards--few (columns:auto; max-width:760px) when a category has at most 2 cards and no search is active. AC5/6 of spec T5: the open question (is the SAT group the same set as the SAT page) is answered in spec T5 section 3: one source (setData, catFor === sat), two renderings; no change to how the pages share setData. Evidence: tests/webui/hscroll-and-rail-2960.test.mjs red 0/4 on origin/dev data (no hscroll class; rail items 54 px high with the icon on its own line, REBOOT text badge) and green 4/4 on the fix; a11y-structure 12, footer-copyright 2, about-cards-typography 2, otd-hidden-in-pic-mode 2, contrast-tokens 3, ble-nopsram-warning 3, i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, sat-areas-zones-groups 2, sat-page-order 1, sat-settings-locked-when-off 6 pass; evaluate.py --quick 88 pass 0 fail (Design-System Class Drift clean for .hscroll, .ri-ic, .ri-lbl, .ri-reboot, .set-cards--few). Screenshots before/after (synthetic data): OTGW-build-artifacts-safe/evidence-TASK-2960/. Not verified: a real phone or unit; SAT views and the Home chips fade only when their content overflows (the SAT views fit at 390 px today), covered by the invariant check in the test, not by a screenshot; the NAS full suite (09).
<!-- SECTION:NOTES:END -->
