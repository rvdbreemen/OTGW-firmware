---
id: TASK-2957
title: >-
  SAT page paused-state honesty: one status, paused markers, burner-cycle Off
  text (UX review F3 F7 F12)
status: To Do
assignee: []
created_date: '2026-11-20T14:17:00Z'
updated_date: '2026-11-20T14:50:00Z'
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
- [ ] #1 otgw-dev's answers to Q1-Q4, Q6, Q7 and the Q5 confirmation of docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T2-sat-display-states.md section 6 are in the notes; where an answer changes a condition in section 2 the spec is updated first
- [ ] #2 Each of the 10 display states of spec T2 section 2 renders the pill, headline, sub-line and card treatment of section 3, checked by a browser test with one status object per state; during TAKEOVER the switch label reads 'Off · takeover active'
- [ ] #3 With enabled true, ot_link_lost true and boiler_status idle (the .63 case) the headline never shows 'At target' or 'idle'
- [ ] #4 The IDLE headline follows spec T2 section 3.1; with room 25.5 and target 19.8 it reads 'Idle · room above target'
- [ ] #5 The messages badge shows visible text ('1 message' / 'N messages') and keeps its aria-label
- [ ] #6 The Burner cycle Off sub-line is 'SAT is not actively controlling.'
- [ ] #7 New EN keys exist; NL and DE are filled by the i18n sync and tests/webui/i18n-*.test.mjs pass; sat_big_safety, sat_big_at_target and sat_cycle_sub_off keep their key names and get re-translated NL and DE values, visible in the sync diff
- [ ] #8 No change outside src/OTGW-firmware/data/ and its tests
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T2-sat-display-states.md (display states, copy EN, edge cases, Q1-Q7 for otgw-dev). ACs replaced by the spec's acceptance list. Correction to review F3: the messages badge already has aria-label 'Messages: N' (v2.js:7293); only visible text is missing.
<!-- SECTION:NOTES:END -->
