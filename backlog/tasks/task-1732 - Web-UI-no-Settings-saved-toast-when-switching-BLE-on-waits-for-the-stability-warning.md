---
id: TASK-1732
title: >-
  Web UI: no 'Settings saved' toast when switching BLE on waits for the
  stability warning
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T15:11:00Z'
updated_date: '2026-11-08T22:55:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 839000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the TASK-1693 bench (.61, beta.148): when satbleenable is the only dirty key on a board without PSRAM, saveSettings() removes it to show the stability warning, posts nothing, and still shows the green 'Settings saved' toast. Cosmetic but misleading. Show no toast (or a neutral one) when nothing was posted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With only satbleenable dirty on a no-PSRAM board without ack, Save shows the warning and no 'Settings saved' toast; a browser test is red on beta.148 and green on the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 host-verified (build/bench pending a slot): v2.js saveSettings() shows 'Settings saved' only when keys.length > 0 (a save holding only satbleenable for the BLE warning or satenabled for the SAT wizard posted nothing). tests/webui/ble-nopsram-warning.test.mjs test 2 now also asserts no 'Settings saved' toast while the warning waits: on origin/dev (beta.148) FAIL 'no Settings saved toast when nothing was saved: Settings saved'; on the fix 4/4 in one run, the other two runs lost one test each at page start-up ('v2.js started' / 'the Enable BLE sensors row is shown', before the feature runs; machine under load). settings-error-toast 4/4, settings-help 29/29, evaluate --quick 0 FAIL.

2026-11-08 review (OTGW agent 3, read-only): MEDIUM maybeShowBleConsent post() swallowed failed POSTs (v2.js 'if (r.ok && then) then(); .catch(function(){})'), so with the 'Settings saved' toast gone a failed satbleenable/satbleriskack store gave no feedback; LOW no test for the satenabled-only wizard path. Fixed: post() shows red 'Save failed (HTTP n)' / 'Save failed'. Tests: ble-nopsram-warning test 'Enable anyway whose save fails' (harness gains { __status } to fail a request) red before the fix ('red Save failed toast: ' empty), green after; settings-error-toast test 'a save that only switches SAT on' red on beta.148 ('no Settings saved toast: Settings saved'), green on the fix. Lane rebuilt on origin/dev beta.149 (patch of own files, old commit kept as branch lane-1732-pre-rebase): ble-nopsram-warning 5/5 (one CDP start-up stall re-run green), settings-error-toast 5/5, evaluate --quick 92 checks 0 FAIL 0 WARN. Build + bench .61 tonight per the coordinator's queue (151 after 150).

2026-11-08 19:0x coordinator renumbering: TASK-1731 takes beta.151 (with 150), so TASK-1732 = beta.152; re-bump at the final rebase (lane commit daf1fed26 currently carries beta.151). (carried over from the main dev tree copy by otgw-firmware-4c, 2026-11-08)

2026-11-08 int-157 (combined build plan, Robert approved): rebased onto int-157 e8b96e689 (150+151), bumped to beta.152, commit b5c19dbc3; int-157 moved there and handed to agent 1. Gates on that tip: ble-nopsram-warning 5/5, settings-error-toast 5/5, evaluate --quick 92 checks 0 FAIL 0 WARN. Bench on the combined image (54 builds/flashes .61) pending. (carried over from the main dev tree copy by otgw-firmware-4c, 2026-11-08)

2026-11-08 otgw-firmware-80 red/green on int-157 (HEAD e4d0280f4) vs origin/dev 07b38ee49 + the new test file: settings-error-toast fix 5/5, old 4/5 (fails: save that only switches SAT on posts nothing, no Settings saved); ble-nopsram-warning fix 5/5, old 3/5 (fails: switching BLE on shows the warning then stores enable+ack; failed Enable-anyway save says so).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: settings-error-toast 5/5 on the fix, 1 red on origin/dev 07b38ee49 (the save posts nothing, no 'Settings saved'); ble-nopsram-warning 5/5 on the fix, 2 red on old. Bench .61 (no PSRAM) ran beta.157 for a 15-min soak without issue. Pushed in 8b67ad039, posted as beta.157.
<!-- SECTION:FINAL_SUMMARY:END -->
