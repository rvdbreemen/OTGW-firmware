---
id: TASK-1309
title: 'Update page: cut the text to what a user needs to update'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T07:51:00Z'
updated_date: '2026-10-23T08:10:00Z'
labels:
  - webui
  - update
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 432000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 07:33 (#alpha-testing, screenshot): the 'OTGW firmware Flash utility' update page is far too verbose (intro, Kept/Starts fresh lists, a blue info block about heating control, auth and DHCP, then a status line). Keep it simple: show only what is essential to update, and do not describe in text what is obvious. Keep the safety-relevant facts reachable (keep the tab open, heating not controlled for 1-2 min on OT-Direct) but short or behind a details toggle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The page shows at most: a one-line purpose, the file chooser with a short hint, the Update button, the unit/version line and one short warning line; the rest moves into a collapsed details element or is removed (before/after screenshot)
- [x] #2 The update flow still works end to end on the bench (safeboot handover, firmware + web files, Done) after the text change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Edit the PROGMEM page in src/OTGW-firmware/updateServerHtml.h (#oneIntro ~88-96): visible = one line 'Choose the release zip (or its .ino.bin and .littlefs.bin) and press Update. Keep this tab open until it says Done (2 to 3 minutes).', one short warning line 'The unit does not control heating while the updater runs (1 to 2 minutes).', the unit/version line; the Kept / Starts fresh list and the notes (OTGW32 vs PIC during the update, admin password asked again, DHCP address may change) move into a collapsed <details> ('Details: what is kept, what to expect'), texts shortened but the Kept/Starts fresh <li> content unchanged (tests/test_fs_update_keep.py and tests/webui/update-orchestrator.test.mjs check it). Drop-zone hint shortened. No JS/flow change. Verify: tests above + node update-orchestrator test, before/after screenshot + visible word count (before 275), build, flash app on the bench, one full update run through the page (safeboot handover, firmware + web files, Done). Firmware path -> prerelease bump.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 10:10 evidence: before (alpha.462 bench) #oneBox 275 visible words (before-update.png); after 87 visible words (after-update-final.png): purpose + duration line, keep-tab-open, one warning line (heating + OTGW32 boiler on own settings), unit/version line, collapsed Details (Kept / Starts fresh unchanged, PIC note, password/DHCP note), file chooser, Update. tests/test_fs_update_keep.py OK, tests/webui/update-orchestrator.test.mjs 58/58. Full one-step update through the page with build/...alpha.462+3f68317-flash.zip (first text version, same JS): 'Done. The unit runs 2.0.0-alpha.462+3f68317 with matching web files.' after 27 s (firmware already matched; it replaced the stale web files), 178 settings unchanged. Reviewer: no defects; two clarity points applied (duration sentence, OTGW32 consequence visible), then rebuilt and re-flashed for the final screenshot.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The update page ('OTGW firmware Flash utility') is cut from 275 to 87 visible words: one line with the purpose, the duration and 'keep this tab open', one warning line (no heating control for 1 to 2 minutes; on an OTGW32 the boiler runs on its own settings), the unit/version line, the file chooser and Update; what is kept or reset and the PIC/password/DHCP notes are in a collapsed Details section. Tester feedback (gruijter, 'keep it simple'). Evidence: before/after screenshots and word counts on the bench (%TEMP%/bench-1309), Kept/Starts fresh tests green (test_fs_update_keep, update-orchestrator 58/58), a full one-step update through the page ended in Done with matching web files and unchanged settings, evaluate --quick 0 fail. Not verified: the update run used the first text version of the page (identical JS and flow; only wording changed after it).
<!-- SECTION:FINAL_SUMMARY:END -->
