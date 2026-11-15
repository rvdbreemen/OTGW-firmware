---
id: TASK-1920
title: >-
  The filesystem image ships a default settings.ini that differs from the
  firmware's own first-boot defaults and makes a fresh unit look like a 1.x
  upgrade
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T04:28:00Z'
updated_date: '2026-11-15T21:07:00Z'
labels:
  - bug
  - settings
  - filesystem
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 985000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1774 (code reading, not seen on a device). src/OTGW-firmware/data/settings.ini (400 B) is part of the LittleFS image: the beta.233 littlefs.bin contains it (grep -a), build.py stage_data_dir() copies data/ whole. Compared with what readSettings() writes when the file is missing (settingStuff.ino:571-576, writeSettings() from the struct defaults): (1) MQTTenable false in the file, true in the struct (MQTTstuff.h:54); (2) OTGWcommands GW=1 in the file, empty in the struct (PICtypes.h:81); (3) the file has no MQTTuseLegacyOtTopics key, so readSettings() takes the 1.x-upgrade path and turns legacy OT topics on (settingStuff.ino:654-658), where a missing file gives new topics; (4) the file has no ui_onboarded key, so the first-time-setup wizard is marked done (settingStuff.ino:671-675), against the TASK-997 comment that a fresh install has no settings file; (5) no SATbleenable key: the no-PSRAM BLE default applies either way. Hostname OTGW2 and broker homeassistant.local match the struct. Consequences: a unit flashed with an image (USB full flash, or any filesystem OTA before its restore) starts on this file, not on the firmware defaults; and every filesystem OTA briefly replaces the user's settings.ini with it (TASK-1774 / ADR-233 cover the reset case). Options for Robert: A remove data/settings.ini from the image (first boot then uses the struct defaults: MQTT on, no GW=1, new topics, wizard shown); B keep it but bring it in line with the struct defaults and add the missing keys; C keep as is (document that a fresh flash behaves like a 1.x upgrade). Not changed in TASK-1774.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Verified on a bench unit what a fresh image flash starts with (settings, MQTT topic naming, wizard state), against a unit whose settings.ini was deleted
- [x] #2 Robert chose A, B or C (put by the coordinator with the consequences above)
- [x] #3 The chosen option is implemented with a host or build check that fails on the old image content
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Released by Robert (DM 1571454228763779093, relayed by the coordinator 2026-11-15): needs-maintainer-ok removed; queued for session 95 after the lane-95-next build/push.

Analysis (session 95, 2026-11-15, code read): the shipped data/settings.ini (14 keys) differs from the struct defaults in MQTTenable (false vs true), OTGWcommands (GW=1 vs empty) and lacks MQTTuseLegacyOtTopics and ui_onboarded, so readSettings() takes the 1.x-upgrade path (:654-658 legacy topics on) and skips the wizard (:671-675). New fact: the GW=1 in the file never ran: picBoot.bEnable defaults to false (PICtypes.h:83), the file has no OTGWcommandenable key, and sendPICBootCommands() returns when disabled (OTGW-Core.ino:1539-1540). Option A therefore loses nothing functional. Side fact (not this task): the PIC's factory EEPROM is monitor mode (gateway.asm 6.8 Configuration de 0 :5471, SetMonitorMode :4750-4756), so a brand-new PIC needs GW=1 once, as before. Question A/B/C put to Robert through the coordinator with recommendation A (remove the file from the image; one source of defaults). Waiting for the answer (AC2) and for a bench slot for AC1.

Decision: coordinator under Robert's delegation (CLI 2026-11-15: 'only ask what we really can't'), option A: remove data/settings.ini from the image; B (align the file + an equality test, defaults in two places) and C (keep, document a fresh flash as a 1.x upgrade) considered. Implemented: git rm src/OTGW-firmware/data/settings.ini; tests/webui/update-orchestrator.test.mjs now lists settings.ini among the runtime files that must not be in the image (it asserted the opposite); tests/test_wifi_deadlink.py and test_wifi_skip_portal.py take the 16 header bytes writeSettings() writes as a literal instead of reading the shipped file; docstring of test/host/test_fs_ota_rtc_snapshot.py, docs/guides/WIFI_FIRMWARE_UPDATE.md (image holds no settings.ini), CHANGELOG Changed line; manual ch02/h02 note that a factory-new PIC starts in monitor mode and needs GW=1 once. AC3 evidence: new tests/test_fs_image_settings_defaults.py (no settings.ini in data/ or added by stage_data_dir(); readSettings() writes and re-reads the defaults and writeSettings() writes MQTTenable/MQTTuseLegacyOtTopics/ui_onboarded; struct defaults MQTT on, new topics, wizard not done): 3/3 OK, test_1 FAIL with the file back. Sweep 27 tests touching data/ or the image green. AC1 (bench: what a fresh image flash starts with) open, low priority in 09's queue. Release-notes line to 92: 'A freshly flashed unit now starts with the normal defaults (MQTT on, new topic names, setup wizard) instead of looking like a 1.x upgrade.'
2026-11-15 15:58-16:12 bench 09 on .61, beta.320+5b9efcc (image without data/settings.ini). (a) filesystem OTA over existing settings: ota_flash 'settings: 165 keys, changed: none', REST diff [], settings.ini diff empty (ADR-233 RTC restore). (b) 'fresh unit': saved settings.ini, deleted it (/api/listfiles?delete=/settings.ini -> 'File deleted', GET /settings.ini 404), reboot: the unit came back with the firmware defaults: hostname OTGW2, MQTTenable true, MQTTuseLegacyOtTopics false, ui_onboarded false, mqttbroker homeassistant.local (REST and the newly written settings.ini agree); WiFi kept (credentials in NVS, WifiStaticIP empty). Restore: saved settings.ini uploaded (POST /upload path=// -> 303, byte-identical), reboot, hostname marvin-otgw32, REST diff [] and settings.ini diff vs before 320 empty. Not done: a full USB erase + fresh image flash (would wipe the WiFi credentials); the deleted-ini boot plus this image (no settings.ini inside) stands in for it. Evidence: 09 scratchpad b320/fresh (defaults.json, defaults.ini, saved/after files).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Option A (coordinator under Robert's delegation): data/settings.ini removed from the LittleFS image; a fresh unit now starts on the firmware's own defaults. AC1 (09, bench .61, beta.320+5b9efcc): with settings.ini deleted the unit booted on the struct defaults (MQTTenable true, MQTTuseLegacyOtTopics false, ui_onboarded false, hostname OTGW2); a filesystem OTA over existing settings changed nothing (165 keys, REST diff [], ADR-233 restore); red/green image evidence by 09 on 317 vs 323 in the notes. AC2: option A chosen. AC3: tests/test_fs_image_settings_defaults.py 3/3 OK, test_1 FAIL with the file put back; update-orchestrator.test.mjs lists settings.ini as a runtime file that must not be in the image; sweep of 27 tests touching data/ or the image green. Not done: a full USB erase + fresh image flash (would wipe WiFi credentials); the deleted-ini boot on an image without settings.ini stands in for it.
<!-- SECTION:FINAL_SUMMARY:END -->
