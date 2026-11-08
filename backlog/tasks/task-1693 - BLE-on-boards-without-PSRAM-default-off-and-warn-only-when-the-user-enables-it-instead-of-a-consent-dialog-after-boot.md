---
id: TASK-1693
title: >-
  BLE on boards without PSRAM: default off and warn only when the user enables
  it (instead of a consent dialog after boot)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T12:29:00Z'
updated_date: '2026-11-08T15:09:00Z'
labels:
  - ble
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 808000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs in #alpha-testing 2026-11-07 12:18 UTC (msg 1568600646901305449): instead of warning as soon as a 4 MB / no-PSRAM gateway boots, warn only when the user turns BLE on, and keep it off at boot. Sergeant D 12:20 UTC (msg 1568601041937764453): 'Or even better do not give user the option to enable BLE'. Today: settings.sat.bBleEnable defaults true (SATtypes.h:604), bleActive() needs psramFound() or bBleRiskAck (SATble.ino:636), and v2.js:2884 shows the consent dialog when psram==0, ble_enable and no risk_ack, which a fresh no-PSRAM unit hits on first visit. Changes ADR-169 (PSRAM-aware BLE default with consent gate): Robert's call, a superseding ADR if changed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision by Robert: keep ADR-169, default off on no-PSRAM with a warning on enable, or hide BLE on no-PSRAM boards
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-08: on boards without PSRAM BLE is off by default with no dialog after boot; when the user switches BLE on, show the stability warning once and store the ack (bBleRiskAck). PSRAM boards unchanged; existing installs keep their stored value. Needs a new ADR superseding ADR-169 (adr-kit).

2026-11-08 implemented (host-verified; build/bench pending a slot from 6b). Firmware: settingStuff.ino [ble-nopsram-default] applyBleNoPsramDefault(); fresh-file branch applies it before the first writeSettings(); sentinel g_sawBleEnableKey (set by the SATbleenable setter) -> a file without the key gets BLE off on a no-PSRAM board, persisted by the deferred flush. Web UI v2.js: saveSettings() takes satbleenable out of the save and opens maybeShowBleConsent() when switching on with no PSRAM and no ack (bleNeedsRiskAck()); accept -> satbleenable then satbleriskack sequentially; keep off -> nothing stored; fetchBle() no longer opens the dialog; a paused install shows a hint + 'Read the warning' button in the BLE card. ADR-222 Proposed (supersedes ADR-169 on acceptance; links set then). Tests: tests/test_ble_nopsram_default.py old 4/4 FAIL, fix 4/4 OK; tests/webui/ble-nopsram-warning.test.mjs old: tests 1-3 FAIL (modal on its own; BLE stored before consent), test 4 (PSRAM) OK; fix 4/4 OK in 3 of 4 runs (one run lost 2 tests at page start-up, the known harness/machine flake; 2 further runs 4/4). settings-help 29/29, settings-error-toast 4/4, evaluate --quick 0 FAIL.

2026-11-08 build + bench: esp32-combo beta.148+4a00ce2 (3 stages SUCCESS, artefacts 16:04-16:05). OTA app+fs to .61 (no PSRAM), settings 164 keys unchanged. Browser bench on the real device (ui_1693.mjs, session tmp): A paused install (BLE on, no ack): no modal on the Sensors page, BLE card hint 'BLE is paused'; B BLE off + no ack, switch on + Save: warning opens, Keep BLE off stores nothing (en=false, ack=false); C switch on + Save, Enable anyway: en=true, ack=true stored; original BLE values restored (en=false, ack=true), settings diff none. Screenshots A-paused-hint.png, B-warning.png. Found: when satbleenable is the only dirty key, saveSettings() still shows the 'Settings saved' toast although BLE waits for the warning (cosmetic; follow-up).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 (Robert's decision): on boards without PSRAM BLE is off by default with no dialog after boot; the stability warning shows once when the user switches BLE on; PSRAM boards and stored values unchanged (TASK-1693 notes, ADR-222 Accepted by Robert in session 2026-11-08, supersedes ADR-169). Implementation: settingStuff.ino applyBleNoPsramDefault() for a fresh install and for a file without SATbleenable (sentinel g_sawBleEnableKey, deferred flush); v2.js saveSettings() takes satbleenable out and opens maybeShowBleConsent() (accept -> satbleenable then satbleriskack, keep -> nothing), fetchBle() no longer opens it, paused installs get a hint + button. Evidence: tests/test_ble_nopsram_default.py old 4/4 FAIL, fix 4/4 OK; tests/webui/ble-nopsram-warning.test.mjs old tests 1-3 FAIL, fix 4/4; evaluate 0 FAIL; build beta.148+4a00ce2; bench .61 (no PSRAM) PASS on all three scenarios with settings diff none (see notes). Not verified: a fresh install on hardware (would need wiping the settings; covered by the host test) and a PSRAM board (unchanged path). Known cosmetic: 'Settings saved' toast when BLE was the only dirty key (follow-up).
<!-- SECTION:FINAL_SUMMARY:END -->
