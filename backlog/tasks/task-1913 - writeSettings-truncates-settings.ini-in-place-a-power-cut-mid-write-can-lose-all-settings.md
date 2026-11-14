---
id: TASK-1913
title: >-
  writeSettings truncates settings.ini in place: a power cut mid-write can lose
  all settings
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-13T23:14:00Z'
updated_date: '2026-11-14T08:00:00Z'
labels: []
dependencies: []
priority: medium
ordinal: 979000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1912, not reproduced. writeSettings() (settingStuff.ino:288) opens /settings.ini with mode w, which truncates before the new content is written. A reset or power cut between the truncate and the close leaves an empty or partial settings.ini, and the next boot runs on defaults (WiFi, MQTT, SAT). The TASK-1495 check (settingStuff.ino:521) only detects a short write afterwards; the old file is already gone. restAPI.ino:1813-1831 and the TASK-1912 Dallas labels flush use the safe pattern: write a temp file, then LittleFS.rename() over the original.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 writeSettings() writes a temp file and renames it over /settings.ini; a failed write keeps the old file
- [x] #2 Host test: a write cut off after the truncate leaves the old settings readable (red on the old code, green on the fix)
- [ ] #3 Bench: settings survive save + reboot; a reset during a save keeps a valid settings.ini
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Premise checked against the framework (2026-11-14): (1) esp_littlefs 1.20.3 (framework-arduinoespressif32-libs/esp32s3/dependencies.lock) vfs_littlefs_open() calls esp_littlefs_file_sync()/lfs_file_sync() for every file opened for writing, so open(SETTINGS_FILE, 'w') puts the truncated (empty) file on flash at once; (2) CONFIG_LITTLEFS_FLUSH_FILE_EVERY_WRITE is not set (framework sdkconfig) and Arduino VFSFileImpl::write() only fwrite()s, writeSettings() never calls flush(), so the content reaches flash only at close(). A reset in between leaves a 0-byte settings.ini. Source for (1): esp_littlefs v1.20.3 src/esp_littlefs.c, read via a fetch summary, single source.

Rename semantics: Arduino VFSImpl::rename() (FS/src/vfs_api.cpp:103-138) passes straight to ::rename(); esp_littlefs vfs_littlefs_rename() does no unlink of the destination, returns EBUSY when src or dst is open, then lfs_rename(); littlefs lfs_rename_() (lfs.c, master) deletes an existing destination, creates the new name and (same metadata pair) deletes the source in ONE lfs_dir_commit; DESIGN.md 'The move problem'/'Global state': a cross-pair move is resolved after power loss via the global move state. Caveat: lfs.c/DESIGN.md were read on littlefs master, not on the exact littlefs bundled in esp_littlefs 1.20.3.

Fix: fsCommitTempFile(tmp, path, expected) (helperStuff.ino, marker block) renames a closed temp file over path only when it holds exactly expected bytes, else removes it and returns false. writeSettings() writes /settings.tmp (SETTINGS_TMP_FILE, OTGW-firmware.h) and commits through it (the TASK-1495 size check moved into the helper). updateRebootCount() writes /reboot_count.tmp the same way (same weakness, found on request: a brown-out at boot reset the count to 1). updateRebootLog() already wrote a temp file but removed the old log before the rename: remove dropped, rename checked. Security: the temp file holds httppasswd/MQTTpasswd (ADR-056), so the FSexplorer handleFile() guard now covers SETTINGS_TMP_FILE and setup() removes a stale one right after the mount, before startWebserver(). Trade-off: a save now needs room for both files; on a nearly full FS it fails and keeps the old file (was: wrote a short one). A failed save (also EBUSY while settings.ini is being downloaded) returns false; the existing TASK-1495 settingsMarkDirty() retry covers it.

FS-OTA path checked: no temp name in KEEP_LIST or data/; during bFsImageWriting LittleFS is unmounted, so writeSettings() fails the open and stays dirty, as before; after the remount the update server calls writeSettings() into the fresh image, which now goes through the temp file. _restoreKeptFiles() (OTGW-ModUpdateServer-esp32.h:704) still writes with 'w' into the fresh image: left alone on purpose, a reset there loses the image anyway.

Evidence: test/host/test_settings_atomic_write.py (+ .cpp) compiles the revision's own writeSettings() (key lines replaced by a stub body), updateRebootCount() and the fsCommitTempFile block against a LittleFS model (truncate on flash at open, content at close, atomic rename, EBUSY while open, power cut = exception mid-write). 'python test/host/test_settings_atomic_write.py --old-rev 4210b6481': FIX PASS 10/10 (W1-W4, R1-R2 runtime; S1-S4 static); OLD 4210b6481 fails W1 (settings.ini 0 B after a power cut), W3 (300 B after a full FS), W4, R1 (reboot_count.txt empty), S1-S4; mutants MA1-MA4 each fail exactly their cases; RESULT PASS. S1-S4 are static source checks. tests/test_fs_update_keep.py TASK-1495 assertion moved to the helper; tests/fixtures/web_file_scheduling.cpp gets the SETTINGS_TMP_FILE define. Green: fs_update_keep, ota_flash, fs_upload_boot_writes, hoisting, static_integrity, reboot tests, crashlog, health_no_fs_flash_ops, rest_fs_flags_loop, webserver_listener_retry, settings_float_roundtrip, flash_migrate_settings_compare, fsexplorer_relative_urls, web_file_scheduling, sat_preset_persist, refresh_storm, board_detect_persist_gate, legacy_pin_defaults; evaluate.py --quick 85 PASS 0 FAIL. Not verified on hardware yet: AC3 (09).

Follow-up filed: TASK-1916 (remove-before-rename in writeOtThermoFile/writeOtBoilerFile and the boot-detect persist), kept out of 1913 on the coordinator's word. 95's TASK-1912 flushDallasLabelsLocked() (sensors_ext.ino, dallasLabels block) has its own temp+rename; after 95 lands, switch it to fsCommitTempFile() so there is one helper. 95 read the rename semantics on joltwallet master, I read v1.20.3 and littlefs master; neither is our precompiled lib (component hash 1808d73e...), so the bench (09) saves settings twice in a row and reads /settings.ini back: the second content must be there (rename over an existing file, on our binary). fd check: every handle on the temp file is closed before the rename (writeSettings closes file, fsCommitTempFile closes its check handle, updateRebootCount and updateRebootLog close both handles); a foreign open fd on the target (FSexplorer download) gives EBUSY -> false -> settingsMarkDirty() retry (host case W4).

Caveat closed for the library (bench evidence, not reading): 09 bench 2026-11-14 ~02:30, 95's TASK-1912 step 4, .61 on beta.233: temp file + LittleFS.rename over the existing /dallas_labels.ini, then reboot: new names persisted, only dallas_labels.ini (103 B) left, no .tmp. That proves rename-over-existing works on our precompiled esp_littlefs, so dropping remove() before rename() in TASK-1913 relies on verified behaviour. Still to bench on the 1913 build itself: save settings twice and read /settings.ini back (second content present), no /settings.tmp after boot.

Rebased onto origin/dev fe6d35b59 (d3's beta.232), re-bumped to beta.233 (coordinator order: 1913 before 95's 1912; flushDallasLabelsLocked() stays with 95, who switches it to fsCommitTempFile() after this push). After rebase: harness --old-rev fe6d35b59 OLD fails R1,S1,S2,S3,S4,W1,W3,W4, FIX PASS, MA1-MA4 each fail exactly their cases, RESULT PASS. run_tests.py on 19 patterns: 22 files, 22 passed, 0 failed. evaluate --quick: 92 checks, 85 PASS, 0 FAIL, 7 info. TASK-1916 number checked free on origin/dev.

Bench by 09 on .63, 2.0.0-beta.233+6afbade (evidence in 09's scratchpad b233ba\), PASS: settings round-trip diff shows only the changed field; save A then B coalesced, /settings.ini shows B; no /settings.tmp and no /reboot_count.tmp after the saves and after reboot; reboot_count +1 on each of 3 reboots, one reboot_log line each, older lines kept; 0 times '[Settings] Error: short write or rename failed' over 7 flushes. AC3 is for 09 to tick.

Not verified on the device: (1) brief step 6, the FSexplorer auth guard on /settings.tmp, was skipped because .63 has no HTTP password. The guard is covered only statically: host harness case S3 checks the handleFile() guard names SETTINGS_TMP_FILE, and mutant MA4 (guard drops the temp file) fails S3. No runtime 401 seen. (2) The 'reset during a save' half of AC3 cannot be run on the bench; host harness W1/W3/R1 carry old-vs-fix (OLD fe6d35b59 red, FIX green).

Pushed 6afbade55 (beta.233) to origin/dev on fe6d35b59. Zip OTGW-firmware-esp32-combo-2.0.0-beta.233+6afbade-flash.zip, sha256 d83553017152dac6848578066eee2d43d81d4a85664d29f62822d847ceb40341, posted to #alpha-testing msg 1570984895382163647.

Bench by otgw-bench-tester (09), .63 Classic + PIC 6.8, 2.0.0-beta.233+6afbade (app-only OTA), 2026-11-14 03:05-03:30, evidence 09 scratchpad b233ba\. AC3 first half PASS: settings survive save + reboot (one setting changed -> settings diff after /ReBoot = only that field; A then B saved back to back -> one coalesced flush ~0.7 s later, /settings.ini 5659 B holds B; no .tmp files after saves + reboot; 3 reboots -> reboot_count +1 each and one reboot_log line each; 0 '[Settings] Error: short write or rename failed' in telnet over 7 flushes incl. 3 doRestart flushes). AC3 second half NOT bench-proven: 'a reset during a save keeps a valid settings.ini'. Every bench reboot goes through doRestart, which flushes the settings cleanly before restarting. A hard reset over USB (RTS/DTR on COM8 USB-Serial-JTAG, the esptool HardReset sequence) did not reset the unit (bootcount and uptime unchanged; .61 untouched too), and the only other route is esptool's download mode, which leaves the unit in the bootloader; not done. The power-cut/reset case therefore rests on the host harness of AC2 only. Step 6 (401 on /settings.tmp) not testable: .63 has no HTTP password (the settings API shows a password=N placeholder); unauth GET /settings.tmp -> 404; the served /settings.ini has all secret fields empty.

2026-11-14 08:10 (09, bench-tester): why the round-A reset trials were invalid, and the reset that works. On the ESP32-S3 USB-Serial-JTAG port the chip resets only while RTS=1 AND DTR=0, and the Windows driver usbser.sys sends the line state only on a DTR change. Round A (and esptool's HardReset class on a port opened with pyserial defaults) left DTR high or never sent the RTS change, so nothing reset: .61 bootcount stayed 629. Working in-process sequence (scratchpad tools/reset1913b.py usb_reset): open with dtr=False rts=False; rts=True + dtr=False write; 200 ms; rts=False + dtr=False. .61: bootcount 629->630, lastreset 'USB reset (host)', API back after 13.5 s; opening the port alone does not reset. Calibration on .61 (6 saves, no reset): flush line 2.02-2.17 s after the POST reply; write Flushing->saved 89-392 ms (device timestamps). Robert chose B (DM 1571058018039242833): trials on .63 triggered on the telnet flush line, offsets 0-350 ms step 50 plus 500 control, 5 reps; fail = any MIX/DEFAULT settings.ini or bootcount not +1.

2026-11-14, maintainer decision relayed by the coordinator: Robert chose B for AC3, no waiver. otgw-bench-tester 09 runs USB-reset trials during a settings save on .63 (esptool's USB-JTAG reset works on .63, seen in the TASK-1774 round-1 incident) after the 2.0.0 public beta and before the TASK-1774 round 2. AC3 stays open until 09's evidence is in.
<!-- SECTION:NOTES:END -->
