---
id: TASK-1719
title: >-
  Web UI: hide every OT-Direct element and setting in PIC mode (board mode 1 and
  3)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T08:25:00Z'
updated_date: '2026-11-08T12:59:00Z'
labels:
  - webui
  - combo
  - ot-direct
dependencies: []
priority: medium
ordinal: 828000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer requirement (Robert, 2026-11-08): when the combo board runs in PIC mode (settings.iBoardMode 1 = Classic S3 Mini or 3 = Classic S3 Mini Pro, or a live PIC), the web UI must never show OT-Direct settings or OT-Direct UI elements. Today sendDeviceSettings() (restAPI.ino ~4389) emits all otd* settings under the compile-time HAS_DIRECT_OT flag, which is always 1 on esp32-combo, so Settings shows the OT-Direct category in PIC mode. The Monitor OT-Direct status/override panels are already gated at runtime on otcommandinterface; device/info emits otdirectavailable=false in PIC mode, which renders an 'OT-Direct (otd*)' group in Device info. Precedent: TASK-865.18 gates the Ethernet settings on bEthernetPresent; TASK-992 hides the PIC tab on OT-Direct boards. Out of scope: the Board mode selector keeps its OT-Direct option (the user must be able to switch); POST of otd* keys stays accepted (backup/restore between boards).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In board mode 1 and 3 (and whenever a PIC is enabled) GET /api/v2/settings contains no otd* key; in board mode 0 or 2 running OT-Direct all otd* keys are present as before
- [x] #2 In PIC mode the Settings rail shows no OT-Direct category and Device info shows no OT-Direct group
- [x] #3 The OT-Direct status and override panels on Monitor stay hidden in PIC mode (regression check of the existing gate)
- [x] #4 A host test fails on the current code and passes on the fix
- [x] #5 Bench: esp32-combo OTA-flashed on a Classic in PIC mode, screenshots of Settings and Device info show no OT-Direct element; an OTGW32 still shows them
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Coordination 2026-11-08: otgw-firmware-6b and otgw-firmware-54 each name the other as 2.0.0 coordinator; go requested from Robert directly. No file overlap reported (6b: TASK-1696B FSexplorer.ino/webServerCompat.h/faviconData.h, TASK-1718 OTDirect.ino; 54: wt-1678 stack touches other v2.js functions). Queue per 54: 6b beta.136 -> 54 TASK-1455 AC7 on .61 (no build) -> 54 wt-1678 build (beta.137+) -> TASK-1719. Classic bench unit .63 (lent by 6b). Review note from 54 on backups: the persistent settings store is settings.ini on LittleFS, written in full by writeSettings() regardless of this gate, so otd* values survive on the device. Only a JSON snapshot via curl GET /api/v2/settings (agent-build-and-bench.md:75, scripts/ota_flash.py:293) taken in PIC mode would lack otd*; restoring that on an OT-Direct board leaves otd* at defaults. Document in CHANGELOG; the web UI has no settings-export button.

Implementation 2026-11-08 (wt-1719, lane-1719 from origin/dev 908207fc6): isOTDirectUiExposed() in OTGW-firmware.h (false when !HAS_DIRECT_OT, or on the combo when iBoardMode is 1/3 or isPICEnabled()); sendDeviceSettings() emits the 14 otd* keys only when it is true; v2.js Device info skips otd* keys when otdirectavailable is false. Accepted scope difference: the Device info gate also hides the single otdirectavailable=false row on a non-PIC board whose OT-Direct init failed (mode 0/2, not PIC mode); the Settings gate does not, so OT-Direct settings stay editable there. ADR context: ADR-201 (detection/persist) untouched, ADR-192 settings.ini read path untouched, no ADR fixes the settings key set. Docs: docs/api/README.md and openapi.yaml state the mode-dependent otd*/eth* groups. Evidence so far: tests/test_otd_settings_hidden_in_pic_mode.py OLD 1 FAIL ((14,14) != (0,0) in mode 1 with PIC) / FIX 2 OK; tests/webui/otd-hidden-in-pic-mode.test.mjs OLD 1 fail ('Device info has no OT-Direct group') / FIX 2 pass, 3 runs in a row; evaluate.py --quick 92 checks, 0 FAIL, 0 WARN.

Full suite 2026-11-08 (run_tests.py --webui -j 4, wt-1719): files 229, passed 216, failed 8, timeout 4. Pre-existing on base 908207fc6 (same result there): test_diag_decode.py (fixture tests/fixtures/diag/recording-v1.bin missing), test_otd_gateway_alt_slots.py, test/host/test_override_reply.py, webui/otd-mode-running-note.test.mjs; environment: test_web_file_scheduling.py (needs host compiler). Load-only, serial green on worktree and base: graph-room-dropout, sat-wizard-tuning-text. Own webui test timed out once under -j 4 (control case 21 s against a 20 s wait, then the harness hung to 600 s like connection-map-text-fits); waits raised to 40 s; rerun under -j 4 with graph-*/connection-map: otd-hidden-in-pic-mode PASS 9.1 s, test_otd_settings_hidden_in_pic_mode PASS 17.9 s (connection-map-text-fits TIMEOUT again, untouched file). Not run separately: test_capture_tool, test_dhw_water_meter, test_sat_lowload_contract (no overlap with changed files). Queue (coordinator otgw-firmware-6b): 54 beta.137-141, 6b TASK-1718 beta.142, then TASK-1719 beta.143+; commit + bump deferred to the slot to avoid banner conflicts; .63 for PIC bench, .61 assigned later for the OT-Direct control.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Combo in PIC mode (board mode 1/3 or a live PIC, also a dead forced PIC) no longer shows OT-Direct settings or elements. isOTDirectUiExposed() (OTGW-firmware.h) gates the 14 otd* keys in sendDeviceSettings() (restAPI.ino); v2.js Device info skips otd* when otdirectavailable is false. settings.ini keeps the values, POST still accepts them. Commit 607b13c87 (beta.145+607b13c, built with build.bat, UV_OFFLINE=1 during the GitHub incident). AC1: test_otd_settings_hidden_in_pic_mode.py compiles the real emission block: old code (14,14) keys in mode 1 with PIC, fix 0 in modes 1/3/dead-PIC/auto-with-PIC and 14 in modes 0/2 OT-Direct. Bench .63 (Classic S3 Mini, PIC 6.8): beta.144 164 settings / 14 otd*, beta.145 150 / 0 otd*, no non-otd key changed, settings.ini (valid JSON, 202 keys) still has the 14 otd* values. AC2: .63 beta.144 rail has 'otd' (14) and Device info group 'OT-Direct (otd*)'; beta.145 neither (docs/evidence/task1719_63_before_beta144_*.png, task1719_63_after_beta145_*.png). AC3: .63 beta.145 Monitor otdStatus and otdOvr display none; webui test checks both states. AC4: webui/otd-hidden-in-pic-mode.test.mjs old data 1 fail ('Device info has no OT-Direct group'), fix 2 pass, also under run_tests -j 4; python test old FAIL / fix OK. AC5: OTA app+fs to .63 (PIC) and .61 (OTGW32, psram 0): .61 keeps 164 settings with 14 otd*, the OT-Direct category, the device-info group and visible Monitor panels (otdStatus/otdOvr inline-block), settings diff none (docs/evidence/task1719_61_otd_control_beta145_*.png). evaluate.py --quick 92 checks 0 FAIL 0 WARN. Full suite: no regressions; pre-existing test_diag_decode fixture missing (TASK-1723); fresh worktrees need git submodule update --init.
<!-- SECTION:FINAL_SUMMARY:END -->
