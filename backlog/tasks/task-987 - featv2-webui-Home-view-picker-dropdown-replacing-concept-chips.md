---
id: TASK-987
title: 'feat(v2-webui): Home view-picker dropdown replacing concept chips'
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-01 22:26'
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
- [ ] #1 Dropdown replaces chips; selection switches A/B/C and persists across reload
- [ ] #2 Keyboard/ARIA: button aria-expanded toggles, menu items reachable, outside click closes
- [ ] #3 python evaluate.py --quick green; build green
<!-- AC:END -->
