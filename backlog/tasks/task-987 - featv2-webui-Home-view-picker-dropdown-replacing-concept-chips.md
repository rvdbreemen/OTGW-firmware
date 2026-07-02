---
id: TASK-987
title: 'feat(v2-webui): Home view-picker dropdown replacing concept chips'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 22:26'
updated_date: '2026-07-02 05:20'
labels: []
dependencies: []
ordinal: 199000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit area home-viewpick-override finding 1; maintainer decision 2026-07-02: adopt the mockup viewpick dropdown as-is (option 1). Replace the .cchips row with the viewpick button (View: name + grid icon + caret) and menu (checkmark items with small descriptions, role=menu/menuitem, aria-haspopup/aria-expanded, click-outside close). Keep the existing localStorage persistence key.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Dropdown replaces chips; selection switches A/B/C and persists across reload
- [x] #2 Keyboard/ARIA: button aria-expanded toggles, menu items reachable, outside click closes
- [x] #3 python evaluate.py --quick green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented directly in main thread (small): replaced .cchips button row with the mockup's .viewpick dropdown (grid icon + 'View: <name>' + caret, menu items with checkmark + descriptions, role=menu/menuitem, aria-haspopup/aria-expanded, click-outside close). All 4 .cchip JS refs migrated to .vp-item (showDesign, activeDesign, renderActive, init wiring) + label sync + closeViewMenu. Kept existing localStorage key otgw-v2-design (persist across reload). Touch-target rule updated cchip->viewpick-btn/vp-item. 0 residual cchip refs, parse OK, drift gate 0 FAIL. On-device verify pending.
<!-- SECTION:NOTES:END -->
