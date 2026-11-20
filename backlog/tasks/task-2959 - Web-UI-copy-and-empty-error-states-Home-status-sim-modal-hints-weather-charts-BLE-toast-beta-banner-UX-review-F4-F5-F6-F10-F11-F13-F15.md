---
id: TASK-2959
title: >-
  Web UI copy and empty/error states: Home status, sim modal, hints, weather,
  charts, BLE toast, beta banner (UX review F4 F5 F6 F10 F11 F13 F15)
status: To Do
assignee: []
created_date: '2026-11-20T14:26:00Z'
updated_date: '2026-11-20T14:51:00Z'
labels:
  - ux
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1093000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F4, F5, F6, F10, F11, F13, F15). F4: Home says 'everything quiet' with no boiler (and 'all good' with no data). F6: sim modal closes on a failed POST with no feedback (v2.js:8519-8535). F10: Appliance-type hint 13 lines in a narrow column. F11: weather card says location detection needs HTTPS (v2.js:1621), which the firmware does not have. F13: charts without an empty state; 'live' label while SAT is off. F15: BLE toast body in mono font, covers cards. F5: SAT beta banner is a bold wall of text with script paths (v2.html:355-357); copy by BlueFlame; the banner disappears at 2.0.0 stable, so drop F5 if stable lands first. Weather wording paired with otgw-docs. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: home-1400-dark.png, home-1400-dark-simmodal.png, mock-home-390-light.png, sat-control-1400-dark.png, mock-sat-control-1400-light.png, settings-1400-dark.png, sat-simple-1400-*.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 F4: Home shows 'Waiting for data from the gateway…' until the first successful /api/v2/otgw/otmonitor answer, 'No boiler connected' with a working 'Check connection ›' while boilerOk is false, and 'everything quiet' only with the boiler link up; the header reads 'checking…' until the first health answer; a browser test covers the four cases
- [ ] #2 F6: a browser test answers the sim enable POST with HTTP 500 (modal stays open, error line names 'HTTP 500', button 'Try again'), with a network error (reason 'no answer from the gateway') and with HTTP 200 (modal closes, toast shows); pending disables both buttons
- [ ] #3 F10: no settings hint renders longer than 2 lines at 1400 px; Appliance type shows the short line and its (i) holds the full text from SET_META.satsource.help; settingsHelp.h unchanged
- [ ] #4 F11: on an HTTP page 'Detect location' is hidden and the hint has no 'HTTPS'; the link opens SAT settings › Weather with Latitude focused (the group heading while SAT is off); otgw-docs uses the same wording in manual §5.9 EN and NL
- [ ] #5 F13: the control-loop chart shows the overlay and label per spec T4 in each state (paused, stopped and starting rows after TASK-2957); the heating curve card shows its one-line empty state at 120 px
- [ ] #6 F15: the discovery card body is in the UI font and spans the width at 390 px; the collapse chip only if the coordinator picks that option
- [ ] #7 F5: the SAT beta banner is the BlueFlame-approved line with one link to docs/sat/field-run-recipe-2.0.0.md, or the item is closed as dropped because 2.0.0 stable removed the banner
- [ ] #8 New EN keys exist; NL and DE filled by the i18n sync, tests/webui/i18n-*.test.mjs pass; keys that change meaning (sat_geo_hint, sat_beta_head/body if built, the satsource hint if translated) get re-translated NL and DE values, visible in the sync diff
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T4-copy-and-states.md (exact copy, states and edge cases per finding). ACs replaced by the spec's acceptance list. Dependency: the F13 paused/stopped/starting rows need TASK-2957's display states; the other rows can land first. F15 collapse chip is an open option for the coordinator. F5 copy drafted, sent to BlueFlame.
<!-- SECTION:NOTES:END -->
