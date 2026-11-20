---
id: TASK-2960
title: Web UI 390 px scroll hints and Settings rail layout (UX review F8 F9)
status: To Do
assignee: []
created_date: '2026-11-20T14:30:00Z'
updated_date: '2026-11-20T15:09:00Z'
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
- [ ] #1 At 390 px the 5 strips of spec T5 section 1 (main nav, SAT views, Monitor/Advanced tabs, Settings rail, Home chips) show the edge fade on the side with hidden content in both themes, the fade follows scrolling, and the active item is in view after a page or view change; before/after screenshots
- [ ] #2 At 1400 px every rail item keeps icon and label on one line, long titles end in an ellipsis with the full title as title, Network shows the reboot dot instead of the text badge, and the 3 short rail titles are used
- [ ] #3 A category with at most 2 cards renders in one column of up to 760 px when no search is active
- [ ] #4 New classes have selectors, python evaluate.py --quick shows no new FAIL, new EN keys go through the i18n sync and tests/webui/i18n-*.test.mjs pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T5-mobile-strips-and-rail.md. ACs replaced. Open question answered: the Settings SAT group and SAT > Control settings are one source (setData, catFor==='sat'), two groupings. New finding: .rail-item is a div with a click handler, not keyboard reachable (WCAG 2.1.1 level A); the coordinator may move that part to TASK-2958.

2026-11-20: the rail-item <div> -> <button> + aria-current part (spec T5 section 2.1, WCAG 2.1.1) moved to TASK-2958 by coordinator decision; TASK-2960 keeps the layout only.
<!-- SECTION:NOTES:END -->
