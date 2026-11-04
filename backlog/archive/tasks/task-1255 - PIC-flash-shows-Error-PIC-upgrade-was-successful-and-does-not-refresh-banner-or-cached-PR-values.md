---
id: TASK-1255
title: >-
  PIC flash shows 'Error: PIC upgrade was successful' and does not refresh
  banner or cached PR= values
status: Done
assignee:
  - '@claude'
created_date: 2026-10-19 17:32
updated_date: '2026-10-26T06:24:00Z'
labels:
  - bug
  - webui
  - pic
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 387000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter in #alpha-testing 2026-10-19 with screenshot: after a successful PIC upgrade (6.6 to 6.8) the v2 Advanced > PIC page shows a red bar 'Error: PIC upgrade was successful'; the header shows 6.8 but the update-available banner still says 6.6 -> 6.8 and the cached PR= table still shows 6.6. Cause (read in code): fwupgradedone() (OTGW-Core.ino ~5796) writes the success text into state.flash.sError; /api/v2/pic/flash-status sends it as error; pollPicFlash() (v2.js ~3421) treats any non-empty error as failure. After the flash only fetchPic runs, not picUpdatePoll/renderPicBanner or fetchPicSettings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Successful flash shows a green/neutral success state, a failed flash still shows the error (old-vs-fix evidence)
- [x] #2 After success the update banner and the cached PR= gateway settings refresh to the new version without a page reload
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (TASK-1255, not built/committed/bumped by this stage):
- Firmware OTGW-Core.ino fwupgradedone(): on OTGW_ERROR_NONE the success text is still logged (Debug 'Message:' + OTDebug 'Upgrade finished' + '*** UPGRADE SUCCESSFUL ***'), then sError is cleared. Readers of state.flash.sError checked: REST /api/v2/pic/flash-status 'error', REST /api/v2/flash/status 'pic_error', WS completion message 'error' (state 'end'). No MQTT, telnet command, script or data/ consumer parses the success text (grep src/ scripts/ tests/ docs/api).
- Success also restarts the PR= readout from index 0 (picSettingsCycleActive=false; triggerPICsettingsReadout()). Finding: the PR= cache was never refreshed after a flash; only boot and GET /pic/settings trigger a cycle, a trigger during an active cycle is ignored, and queryNextPICsetting() returns early while bPICactive without advancing, so a cycle running before the flash resumed mid-way. The PIC version itself comes from the reset banner (OTGW-Core.ino banner path sets state.pic.sFwversion), which is why the header already showed 6.8 in the report. PR= queries are spaced 3 s and NG/SE/TO answers keep the cached value, so an early query during PIC restart is harmless.
- upgradepic action=upgrade resets iPICprogress=0 and sError='' (only when no flash is active): the flash starts from loop() after the HTTP answer, so the first poll could otherwise read the previous flash result.
- v2.js: new pure picFlashOutcome() -> running/success/error/pending; success = progress 100 with empty error OR the legacy 'PIC upgrade was successful' text (old firmware + new UI after an fs-only update). Pending polls up to 10 s then reports 'the flash did not start'. Success: accent bar + 'Flash complete: <file>', then after 2.5 s picAfterFlash(): devInfoTs=0 (forces fresh device/info), fetchPic(done) -> picUpdatePoll(done) -> fetchPicSettings(true), chained one request at a time (done callbacks added to fetchPic/fetchPicFiles/picUpdatePoll). fetchPicSettings(full) keeps re-reading for 60 s even when nothing is missing, so the restarted PR= cycle's values replace the old cache on screen.
- docs/api/README.md: flash-status error/progress semantics.
- Harness tests/webui/pic-flash-status.test.mjs (9 tests, runs the real v2.js functions + a static check of fwupgradedone). OLD (HEAD v2.js + OTGW-Core.ino via OTGW_V2_JS/OTGW_CORE_INO): 1 pass / 8 fail, incl. 'Error: PIC upgrade was successful' and refresh calls only ['fetchPic devInfoTs=12345']. NEW: 9/9 pass. All webui tests 25/25, test_static_integrity OK, evaluate.py --quick 72 pass 0 warn 0 fail.
- Not verified: real PIC flash on hardware, banner timing vs the 2.5 s delay, real PR= cycle duration after flash; firmware not compiled in this stage.

Review round (2026-10-19), fixes in v2.js + openapi.yaml + test:
- Non-OK flash-status replies (503 from the REST/heap gate) no longer count as 'pending'. They get their own cap (PIC_NOSTATUS_MAX=60 polls) and end with a neutral 'No flash status from the device' message, never 'did not start' or a red bar. Tests: 503-burst-during-flash and persistent-503 cases.
- Old firmware + new UI: 'PIC upgrade was successful' counts as success only after this flash was seen running (picSawRunning); before that it is the previous flash's result and reads as 'pending'. Test added; red on the pre-review UI, green now.
- Post-flash update-check passes ?recheck=1, so a PIC type change (gateway/diagnose/interface) does not compare the new version against the old type's cached latest.
- openapi.yaml: progress/pic_progress minimum -1, error/pic_error semantics described.
- Correction to the earlier note 'an early PR= query during the PIC restart is harmless': NG/SE/TO keep the cached value, and that cached value is the OLD firmware's value. A register whose single query in the restarted cycle is lost (or answered NG by the new firmware) keeps showing the pre-flash value. Risk judged small (first query is at the next 3 s tick after fwupgradedone), NOT verified on hardware.
- fetchPicSettings comment corrected: the 3 extra reads are margin for deferred PR= ticks, not PIC restart time. Residual: registers that arrive after the last UI read show on the next tab open; each GET may start a redundant 45 s cycle after the restarted one ends (same pattern as the existing missing-value polling).
Not verified: banner timing vs the 2.5 s refresh delay and the PR=O first-query answer on the Classic-S3 bench; firmware change not compiled in this review step (build left to the orchestrator).

Finalize (2026-10-19): build.bat (esp32-combo) SUCCESS, firmware 2,020,944 B (88.1% of 2,293,760) + littlefs.bin fresh at 20:42, .elf kept as build/OTGW-firmware-esp32-combo-2.0.0-alpha.437+147a717.elf (built before the bump, so the binary banner reads alpha.437). evaluate.py --quick: 72 pass, 0 warn, 0 fail, exit 0. node --test tests/webui/*.test.mjs: 28 pass, 0 fail. pic-flash-status.test.mjs: 12/12 pass. test_static_integrity OK (33), test_webui_asset_versioning OK (2). Bumped alpha.437 -> alpha.438. AC#2 left open: the UI refresh chain is proven in the harness, but the actual banner and PR= values after a real PIC flash need a Classic-S3 bench flash (not done: PIC flashing is not allowed in this step).

2026-10-19 bench verify blocked: bench S3 absent from USB and network (no 303A:1001, .61 no ARP reply). Fix rebased onto origin/dev (PR #687), rebuilt alpha.439+31001d4: build.bat exit 0, firmware+littlefs SUCCESS, evaluate --quick 0 FAIL, all tests/webui pass; pushed to origin/dev. Remaining AC needs on-device evidence.

2026-10-20 bench OTGW32 (no PIC) on alpha.439: GET /api/v2/pic/flash-status -> 503 'No PIC detected - PIC functions disabled' (expected gate); v2 Advanced page renders with 0 page errors, 2 console 503 resource loads (likely the no-PIC gate, URLs not identified). AC2 still needs a real PIC flash on a Classic board.

2026-10-20 maintainer decision: allowed to re-flash gateway.hex 6.8 (same version) on the Classic-S3 bench to prove AC2.

2026-10-22 cycle check: Classic-S3 still not connected (no COM8, 192.168.88.63 no HTTP answer); stays blocked.

2026-10-22 23:15 cycle: skipped, AC2 needs a real PIC flash (explicit maintainer order required).

2026-10-26 07:00 bench, Classic + S3 Mini .63 (MAC AC:27:6E:CE:45:D8, PIC 6.8 pic16f1847, alpha.508), one same-version re-flash of gateway.hex 6.8 from the v2 UI (Robert's permission 2026-10-25). Success state PASS: 'Flash complete: gateway.hex' in ~44 s, accent bar, no error. PR= table PASS: re-rendered without a reload ~16 s after completion. Banner FAIL: stayed on 'Checking for updates…' > 75 s; the recheck=1 GET answered 'checking', the follow-up GET /api/v2/pic/update-check failed net::ERR_CONNECTION_TIMED_OUT, and picUpdatePoll's catch only called done() (v2.js:3807-3815), no retry, no banner text. Device itself answered ready/6.8 by curl. Fix: retry up to 3 times 2.5 s apart, then 'Update check failed' (as checkPicUpdate does). tests/webui/pic-flash-status.test.mjs: 2 new cases with the real picUpdatePoll, 14/14 pass on the fix, 2 fail on HEAD (banner stuck). Separate finding filed as TASK-1406 (reset_cause raw 0xFF after the flash). Screenshots: scratchpad task1255/task1255-01..04.

2026-10-26 06:20 UTC bench re-test on alpha.513+9da1570 (Classic + S3 Mini .63, PIC 6.8 pic16f1847), second and last same-version re-flash of gateway.hex 6.8 from the v2 UI (Robert's permission 2026-10-26). Served v2.js confirmed to contain the retry. Timeline: confirm 06:20:36.220; 'Flash complete: gateway.hex' 06:21:18.436 (no error); banner 'Checking for updates…' 06:21:21.732 -> '✓ PIC firmware up to date (6.8)' 06:21:24.271 and unchanged through 06:23:05; PR= table re-rendered at 06:21:24.290 and re-read every ~5 s for ~40 s, no page reload. All 146 requests 200, console 0 errors. Limits: no update-check GET failed in this run, so the retry branch ran only in the harness (tests/webui/pic-flash-status.test.mjs, red on HEAD / green on fix); same version flashed, so PR= values could not visibly change. Minor (code reading, not observed): a non-OK HTTP reply (e.g. 503) to update-check renders an empty banner (hidden) rather than 'Update check failed'; it does not stick. Evidence: scratchpad task1255-513/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PIC flash from the v2 UI now ends in a clear state. AC#1 (commit 1b818747d): a successful flash shows a neutral/accent success state, a real failure still shows the red error; tests/webui/pic-flash-status.test.mjs decision table. AC#2: after success the banner and the cached PR= settings refresh without a page reload. The first bench run (alpha.508, 2026-10-26 07:00) found the banner stuck on 'Checking for updates…' after one timed-out update-check GET; fixed in alpha.513 (9da1570b1): picUpdatePoll retries up to 3 times, then shows 'Update check failed'. Evidence: harness 14/14 on the fix, 2 failures on the old v2.js; bench re-test on alpha.513: 'Flash complete' -> banner 'up to date (6.8)' 2.5 s after 'Checking…', PR= table re-rendered at +6 s, no reload, 0 console errors (scratchpad task1255-513). Not verified on the device: the retry branch itself (no GET failed in the re-test). Separate finding: TASK-1406 (reset_cause raw 0xFF after a flash).
<!-- SECTION:FINAL_SUMMARY:END -->
