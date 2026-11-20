---
id: TASK-2959
title: >-
  Web UI copy and empty/error states: Home status, sim modal, hints, weather,
  charts, BLE toast, beta banner (UX review F4 F5 F6 F10 F11 F13 F15)
status: To Do
assignee: []
created_date: '2026-11-20T14:26:00Z'
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
- [ ] #1 With no boiler detected Home shows 'No boiler connected' with a link to Connection, never 'everything quiet'; with no status data yet it shows a neutral 'Waiting for data'
- [ ] #2 Browser test: the sim enable POST answered with HTTP 500 keeps the modal open with an error and a retry; HTTP 200 closes it and shows the toast
- [ ] #3 No settings hint is longer than one line under its label at 1400 px; longer help sits in the (i) popover
- [ ] #4 The weather card no longer mentions HTTPS and links to the latitude and longitude fields; the manual uses the same wording (otgw-docs)
- [ ] #5 With no data points both SAT charts show a one-line empty-state message and no 'live' label
- [ ] #6 The BLE toast body uses the UI font and does not cover card content at 1400 px or 390 px
- [ ] #7 The SAT beta banner is one normal-weight sentence with one manual link (copy by BlueFlame), or the item is dropped because 2.0.0 stable removed the banner
<!-- AC:END -->
