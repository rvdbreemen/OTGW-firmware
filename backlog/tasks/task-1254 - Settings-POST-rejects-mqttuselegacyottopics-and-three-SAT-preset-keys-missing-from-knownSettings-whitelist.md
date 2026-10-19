---
id: TASK-1254
title: >-
  Settings POST rejects mqttuselegacyottopics and three SAT preset keys (missing
  from knownSettings whitelist)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-19 17:31'
updated_date: '2026-10-19 19:11'
labels:
  - bug
  - webui
  - rest
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 386000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter in #alpha-testing 2026-10-19 (alpha.436, Classic PIC): toggling 'Legacy OT topic layout' and saving gives 'Save failed' and the change stays unsaved. Cause (read in code): knownSettings[] in restAPI.ino (~4177-4231) lacks mqttuselegacyottopics, satpresetsleep, satpresetactivity, satpresethome, so postSettings() returns 400 Unknown setting name although GET exposes them and updateSetting handles them. Also: the 'Save failed' toast uses the green .toast style (no error variant), and one rejected key aborts the rest of the sequential save chain.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The four keys are accepted by POST /api/v2/settings and persist in settings.ini across a reboot (curl evidence)
- [x] #2 A failed save shows a red error toast, success stays green (screenshot)
- [x] #3 A test fails when a GET /api/v2/settings key that the UI can edit is missing from knownSettings[] (red on old code, green on fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (2026-10-19):
- restAPI.ino knownSettings[]: added mqttuselegacyottopics, satpresetactivity, satpresethome, satpresetsleep. updateSetting() already handles all four (settingStuff.ino 819, 1009-1011); settings.ini write (297, 344-346) and load (readSettings -> updateSetting, line 568) already carry them, so a POST now persists through the 2 s debounce flush. The TASK-648 legacy-default only fires when the key is ABSENT from the file, so a saved value survives a reboot.
- Full GET-vs-whitelist diff (181 emit calls = 181 parsed keys): GET-only = ssid (type r, disabled input in v2.js, legit read-only) + the 4 fixed keys. Whitelist-only = mqttharebootdetection (deprecated ADR-174, kept for old clients) and webhookenabled (alias of webhookenable in updateSetting). Both harmless.
- Error toast: .toast.err{background:var(--status-critical)} in v2.css and v2-bundle.css (bundle verified identical to generate_v2_css_bundle output). --status-critical chosen over --status-danger: white text 5.6:1 light / 11:1 dark vs 2.99:1 for dark --status-danger. bleToast(msg, err, ms) toggles .err; saveSettings success/failure and every failure toast (IP validation refusal, FS delete/upload/not-authorized/choose-file, reboot/reset failed, marker error) now pass err=true.
- Tests: tests/test_settings_whitelist.py (AC3; red on HEAD restAPI.ino naming the 4 keys, green on fix) and tests/webui/settings-error-toast.test.mjs (runs real bleToast+saveSettings; 3/3 red on HEAD v2.js/bundle, 3/3 green). Both documented in tests/README.md. No CI workflow runs them automatically (only adr-governance has a workflow).
- AC2 evidence is a headless-Edge render of the real v2-bundle.css toast in a harness page (light+dark), plus the node test; not a live-UI screenshot.
- Chain abort (NOT changed): saveSettings posts keys sequentially and any non-ok response throws, so (a) keys after the rejected one are never POSTed, (b) keys already saved stay marked dirty (harmless re-POST on retry), (c) the toast does not name the failing key although the Error carries it. Verified with the real saveSettings under a stub fetch: dirty {a,b,c}, b=400 -> posted a,b; still dirty a,b,c. Any other 4xx/401/503 still aborts mid-chain; the new whitelist test only removes the unknown-key cause.
- Open: AC1 needs bench curl (POST + reboot + GET) - not done here (no flashing in this step).

Review round: (1) bleToast hide-timer race fixed: timer id kept on the #toast element, clearTimeout before re-arm; new node test 'a new toast cancels the previous toast's pending hide timer' fails on the pre-fix bleToast (3/4) and passes on the fix (4/4). (2) On-device POST/GET/reboot round-trip of the four keys is NOT yet verified (no build/flash in the review step); the static guard tests/test_settings_whitelist.py only parses restAPI.ino. Final Summary must either carry a bench curl round-trip or state this as unverified.

Finalize (2026-10-19): build.bat (esp32-combo) SUCCESS: firmware 00:06:19, buildfs 00:00:26, 'Build completed successfully!'; fresh artifacts build/OTGW-firmware-esp32-combo-2.0.0-alpha.436+8422465.{ino.bin 20:14:30, elf 20:14:27, littlefs.bin 20:15:00} (pre-bump version string); Flash 88.1% (2020483/2293760), RAM 35.4%; LittleFS budget OK 632 KiB + 96 KiB <= 768 KiB; staged v2-bundle.css in .pio/data-staging contains toast.err. evaluate.py --quick: 79 checks, 72 passed, 0 warn, 0 fail, 7 info, exit 0. tests/test_settings_whitelist.py: 3 tests OK exit 0. node --test settings-error-toast.test.mjs: pass 4 / fail 0. All 7 tests/webui/*.mjs exit 0. node --check v2.js OK. Bumped alpha.436 -> alpha.437. AC1 (on-device POST/GET/reboot round-trip) still NOT verified: no flash allowed in this step.

2026-10-19 bench verify blocked: bench S3 absent from USB and network (no 303A:1001, .61 no ARP reply). Fix rebased onto origin/dev (PR #687), rebuilt alpha.439+31001d4: build.bat exit 0, firmware+littlefs SUCCESS, evaluate --quick 0 FAIL, all tests/webui pass; pushed to origin/dev. Remaining AC needs on-device evidence.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Settings POST /api/v2/settings rejected mqttuselegacyottopics, satpresetactivity, satpresethome and satpresetsleep with 400 'Unknown setting name' because they were missing from knownSettings[] in restAPI.ino, although GET exposes them and updateSetting() handles and persists them. Added the four keys. The 'Save failed' toast used the green style; added a red .toast.err variant (var(--status-critical), white text 5.6:1 light / 11:1 dark) and bleToast(msg, err, ms) toggles it for every failure toast; also fixed a hide-timer race (an older toast's timer could hide a newer toast early).

Evidence per AC:
- AC1 (POST accepted + persists across reboot): NOT verified on-device. Only code reading (updateSetting/settings.ini write/read paths) and the static whitelist test. Needs a bench curl round-trip: POST each key, GET back, reboot, GET again. Task stays In Progress for this.
- AC2 (red error toast, green success): node test tests/webui/settings-error-toast.test.mjs runs the real bleToast+saveSettings from v2.js: 3/3 red on pre-fix v2.js/bundle, 4/4 green on the fix (incl. timer-race regression test). Headless-Edge render of the real v2-bundle.css toast, light and dark (scratchpad toast_light.png / toast_dark.png): green 'Settings saved', red 'Save failed'. Not a screenshot of the live device UI.
- AC3 (test red on old, green on fix): tests/test_settings_whitelist.py against HEAD restAPI.ino FAILED naming the 4 keys (exit 1); on the fix 3 tests OK (exit 0).
Build: build.bat esp32-combo SUCCESS with fresh firmware/littlefs/elf; evaluate.py --quick 72 pass / 0 fail / 0 warn. Version alpha.437.
Known, not changed: one rejected key still aborts the rest of the sequential save chain.
<!-- SECTION:FINAL_SUMMARY:END -->
