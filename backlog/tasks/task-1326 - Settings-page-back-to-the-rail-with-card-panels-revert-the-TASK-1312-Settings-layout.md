---
id: TASK-1326
title: >-
  Settings page back to the rail with card panels (revert the TASK-1312 Settings
  layout)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T17:18:00Z'
updated_date: '2026-10-23T17:38:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 449000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert (number3nl) 2026-10-23 16:52Z on #alpha-testing (msg 1563233772571332700): 'I don't agree with the design change on Setting. Go back to my original design... I want the right side navigation. With the right side card panels with settings... go back 24 hours... the design is optimized for desktop and mobile.' Confirmed in session 2026-10-23: revert only the Settings page to the rail (desktop) + card panels, one category at a time, as before TASK-1312 (commit before ac3a49c16). Keep: Monitor tab memory, Advanced/SAT changes (1310/1311/1322), the deep-link scroll robustness (pendingReveal-style: link opens Sensors and scrolls to the BLE roster even when GET settings is slow), refused-IP toast jumping to the right category. Update the design note: Settings is the exception (rail by maintainer decision).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Settings shows the rail with categories and one category's card panels at a time, as before TASK-1312 (desktop + 390 px screenshots compared with the pre-TASK-1312 look)
- [x] #2 Deep links (BLE Assign, sensor roster) select Sensors and scroll to the roster, also with a slow settings GET; search behaves as before TASK-1312
- [x] #3 Monitor tab memory and the SAT page changes are unchanged (their tests stay green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 19:40 evidence: Settings restored to the rail + one category's card panels (v2.html/.css/.js Settings parts from ac3a49c16^), setOpen/setPeek/otgw-v2-setopen removed; deep links select Sensors and scroll to #setcard-ble via consumeReveal at the end of renderSettings/renderBleCard (slow GET safe); refused IP selects its category and scrolls to the row. Kept: Monitor tab memory, Advanced/SAT .grp, SAT page (1310/1311/1322), TASK-1300/1314 code. Tests: v2-navigation 0 failures of 34 (Settings section rewritten for the rail incl. 800 ms slow GET -> #setcard-ble in view, refused IP -> network); on HEAD assets 15/34 FAIL (no rail, details groups). v2-settings-sat-followups 26, sat-page-order pass, sat-status-pill 14, settings-help 28 (+ tret-pr-events, settings-error-toast, sat-area-sensor-mapping, static-ip-validation, sat-dhw-card, dallas-discovery-toast green per agent), evaluate 0 fail. build.bat SUCCESS (littlefs 19:36), bench fs flash, settings 178/178 differing 0; real browser on the device: rail present, 12 items in SET_CATS order, 0 details.grp. Screenshots docs/evidence/task-1326/ vs scratchpad before-settings-*.png (pre-TASK-1312 look): identical layout.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Settings is back to Robert's design: left rail of categories with the selected category's card panels, one at a time (horizontal rail on narrow screens), as before TASK-1312. Kept from TASK-1312: Monitor tab memory and the robust deep links (BLE Assign / sensor roster land on the roster even with a slow settings GET; a refused IP jumps to its row). Evidence: AC1 rail tests red on the TASK-1312 assets and green now, device DOM check, screenshots match the pre-TASK-1312 look; AC2 slow-GET and refused-IP checks; AC3 SAT/Monitor suites green. Not verified: a real phone.
<!-- SECTION:FINAL_SUMMARY:END -->
