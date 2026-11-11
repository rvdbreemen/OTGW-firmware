---
id: TASK-1819
title: >-
  Firmware OTA: device info names the update path; guard texts and agent rules
  match the safeboot layout
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T20:46:00Z'
updated_date: '2026-11-11T22:36:00Z'
labels:
  - ota
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 908000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-11: make sure marvin (.61) and zaphod (.63) can be fully updated over OTA, device info reports it correctly, and memory/rules say how it really works. Robert decided (AskUserQuestion): keep the TASK-959 guard, fix everything around it. Fact: on the safeboot layout (ADR-191) the only OTA app partition is app0 (ota_0) and it is the running one; IDF esp_ota_get_next_update_partition(NULL) then returns the running partition (esp_ota_ops.c:766-769, default_ota = first OTA slot), so Update.begin() would erase the running app. The guard stays the safety boundary; the app goes through POST /safeboot -> safeboot /update.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 device/info has firmware_ota = direct | safeboot | usb from the real partition checks; host test red on the old code, green on the fix
- [x] #2 Guard log line, error text and comments state the safeboot path on a safeboot unit and the one-time USB migration on an old-layout unit; no 'USB-only' claim on safeboot units
- [ ] #3 Full OTA (app + filesystem) proven on .61 and .63 over WiFi, device info checked on both
- [x] #4 Rules recorded precisely in docs/guides/agent-build-and-bench.md and project memory; stale memory corrected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 implemented (uncommitted, wt-cyc09). Source check: IDF esp_ota_get_next_update_partition (D:/DevData/platformio/packages/framework-espidf/components/app_update/esp_ota_ops.c:737-777) sets default_ota to the first OTA slot and returns it when no later slot follows the running one, so with app0 as the only OTA slot it returns the running partition; Arduino Updater.cpp:234 writes there. Robert chose to keep the guard. Changes: firmwareOtaPathFor/firmwareOtaPath + device/info firmware_ota (restAPI.ino); guard comments, _updaterError texts (95/107 chars, buffer 128) and debug lines name safeboot or the one-time USB migrate; update.html old-layout box points to flash_otgw --migrate; docs agent-build-and-bench.md OTA facts block, c4 docs and WIFI_FIRMWARE_UPDATE.md list firmware_ota. Test tests/test_firmware_ota_path.py (real header functions + IDF-algorithm stub): fix 7/7; old header 5 fail 2 pass; mutant 'nextSlot != nullptr' (no running check) 3 fail. evaluate --quick 0 FAIL; update-orchestrator 58/58, update-auto-return 4/4, test_static_integrity 33/33, test_build_safeboot OK. Current device info (before this build): .61 beta.182 and .63 beta.174 both app_ota_available=False, safeboot_available=True. Memory: project_esp32_combo_4mb_ota_facts RULES block, reference_classic_s3_test_device corrected; .wolf/cerebrum Do-Not-Repeat.

Review (refute-only, opus): no blocking defect; claims checked against IDF esp_ota_ops.c, Updater.cpp:234/523, partitions CSV, flash_otgw.bat:42/92. Fixed: bench-guide wording (ota_flash.py reports the version and exits 2 on a settings change, scripts/ota_flash.py:302-308); comment now says begin() picks the slot and the writes erase it; test now also requires the guard's return before the write (mutant without return: 1 fail). Added firmware_ota/app_ota_available/safeboot_available to docs/api/openapi.yaml. scripts/json-golden/v2_device_info.json is a capture: json_golden.py will list +/firmware_ota until it is re-captured (not changed by hand). Test 7/7, evaluate 0 FAIL.

2026-11-11 beta.185 8ba553b68 pushed (52fab1fb7..8ba553b68), built 23:31 (zip 6,298,735 B), posted msg 1570205496026206378. Bench .61 (OTGW32), as otgw-bench-tester: (1) full OTA app+fs via scripts/ota_flash.py exit 0, fs version.hash 893c5b7 -> 8ba553b, running 2.0.0-beta.185+8ba553b, settings 164 keys changed none. (2) device/info firmware_ota='safeboot', app_ota_available=False, safeboot_available=True, MQTT connected; on beta.184 (same unit, earlier tonight) firmware_ota was absent (None) = old vs new on the device. (3) guard on the device: curl -F update=@<185 .ino.bin> POST /update?cmd=0&size=2130192 to the MAIN firmware -> HTTP 200 'Flash error: firmware goes through the safeboot updater here: POST /safeboot, then upload the .ino.bin there'; bootcount stayed 350, version unchanged, crashlog available=false: refused before any write. Earlier tonight .61 also took full OTAs to 182 and 184+893c5b7 the same way. AC3 open: zaphod (.63) full OTA is scheduled 2026-11-12 06:41 (beta.184+893c5b7, safeboot path), so .63 shows firmware_ota only after a 185+ build lands there. Not verified: the update.html old-layout box (only shown on an unmigrated unit; no such unit on the bench).
<!-- SECTION:NOTES:END -->
