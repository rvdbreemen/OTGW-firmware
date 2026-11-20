---
id: TASK-2957
title: >-
  SAT page paused-state honesty: one status, paused markers, burner-cycle Off
  text (UX review F3 F7 F12)
status: To Do
assignee: []
created_date: '2026-11-20T14:17:00Z'
labels:
  - ux
  - webui
  - needs-dev-input
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1091000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F3, F7, F12). Display only, no change to SAT control behaviour. F3: with SAT enabled and no OpenTherm connection the headline says 'At target · idle' (v2.js:1577) while the sub-line says 'SAT paused' (v2.js:572), the chip says Idle and a red '1' badge has no label. F7: paused Control status values (modulation 100 %, error, PID output) look live. F12: Off text 'SAT is not actively controlling. SAT chooses the mode itself.' contradicts itself (v2.js:1606-1608). needs-dev-input: otgw-dev delivers the state semantics first (which SAT states exist and when the UI should call it paused); otgw-webui builds after that. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: sat-simple-1400-dark.png, sat-simple-1400-light.png, sat-control-1400-dark.png, mock-sat-control-1400-light.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otgw-dev has recorded in this task which SAT states exist and when the UI shows paused (no SAT control behaviour changed)
- [ ] #2 With SAT enabled and no OpenTherm connection, the headline, header chip and sub-line all say paused; no 'At target' or 'idle'
- [ ] #3 The red status badge has visible text or an accessible name naming the problem
- [ ] #4 While SAT is paused, the Control status card title carries '(paused)' and its values use the muted style
- [ ] #5 The burner-cycle Off state shows only 'SAT is not actively controlling.'; PWM and continuous keep the second sentence
- [ ] #6 A browser test drives the paused state and checks headline, chip, sub-line and the paused card
<!-- AC:END -->
