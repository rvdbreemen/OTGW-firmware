---
id: TASK-2960
title: Web UI 390 px scroll hints and Settings rail layout (UX review F8 F9)
status: To Do
assignee: []
created_date: '2026-11-20T14:30:00Z'
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
- [ ] #1 At 390 px each horizontally scrollable strip (main nav, Settings rail, Home chips) shows an edge fade while content is off-screen, and the active item is scrolled into view; screenshots in both themes
- [ ] #2 At 1400 px every Settings rail item keeps icon and label on one line, with or without a badge
- [ ] #3 otgw-webui answers in this task whether the rail SAT group and SAT > Control settings are one source or two
<!-- AC:END -->
