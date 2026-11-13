---
id: TASK-1909
title: >-
  OTA: reboot_count.txt reset to 1 and the app-boot reboot_log entry missing
  after a safeboot OTA (.63, beta.224 release flash 2026-11-13 19:37)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T18:53:00Z'
updated_date: '2026-11-13T23:22:00Z'
labels:
  - bug
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 975000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09 bench: OTA app+fs of 2.0.0-beta.224+b769b8f on .63 (Classic + S3 Mini Pro, PIC 6.8). bootcount went 35 -> 1 instead of 37; /reboot_count.txt holds 1 (on the keep list, OTGW-ModUpdateServer-esp32.h:633, 16 B cap); reboot_log.txt kept old lines but has only the 19:37:32 [OTA] Rebooting entry, the app-boot entry every earlier flash wrote is missing. Same src flashed at 16:22 kept the counter (33 -> 35). Settings intact (150 keys, none changed). Not established: whether sat_tune.json, sat_markers.json, dallas_labels.ini existed before. No telnet capture of this flash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce with a telnet capture across an OTA of app+fs on a Classic bench unit (09), or a host test of the keep/restore path that shows the counter lost
- [x] #2 Root cause named with file:line
- [ ] #3 Fix: reboot_count and reboot_log survive an OTA; red on the old code, green on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Three changes, no restAPI.ino: (1) setup() writes platformResetReason + updateRebootCount() + updateRebootLog() before setupFSexplorer()/startWebserver(), so an FS upload that starts right after the web server is up already finds the new count and boot line in its heap copy. (2) _remountAfterFsWrite() calls LittleFS.end() before begin(), so the image just written is mounted even when some begin() ran during the write. (3) state.flash.bFsImageWriting: set right after the upload's LittleFS.end(), cleared in _remountAfterFsWrite() and on the error-answer path; the four LittleFS.begin() callers in helperStuff.ino (updateRebootCount, updateRebootLog, checklittlefshash on every telnet connect, getFilesystemHash) skip while it is set. Static regression gate tests/test_fs_upload_boot_writes.py on the real sources; bench repro by 09.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 root cause (TASK-1909). (a) OTGW-firmware.ino:532 startWebserver() came before :556-557 updateRebootCount()/updateRebootLog(). The flasher starts the FS upload on the first HTTP answer of the new app, which is inside setup(): 09's captures b227/63 (line 134 '[OTA] Flash start' before line 144 'boot:', which is logged right after :556-557, and before initSAT), b227/61 and b228/61 (Flash start before initSAT). An upload in the :532..:556 window runs _keepFiles() + LittleFS.end() (OTGW-ModUpdateServer-esp32.h:386-387) before the new count and boot line exist; the restore (_restoreKeptFiles via _restoreRuntimeFiles, :487/:532) then writes the old log back without the boot line: matches 'app-boot entry missing'. (b) updateRebootCount()/updateRebootLog() call LittleFS.begin() (helperStuff.ino:143, :458), which mounts the partition while the Updater overwrites it, and write into it. (c) _remountAfterFsWrite() (OTGW-ModUpdateServer-esp32.h:526) then calls LittleFS.begin(), which returns true WITHOUT mounting when a mount exists (framework libraries/LittleFS/src/LittleFS.cpp:58-61 'LittleFS Already Mounted!'), so the version.hash check and the restore ran through the stale mount of the half-written partition. Interpretation, not shown on the flash: (b)+(c) is the likely way /reboot_count.txt ended unreadable so the next boot counted 1; (a) alone would give 36, not 1. The good captures (b227/63) landed just after :557, the same timing class on the other side of the line: intermittent.

Evidence: tests/test_fs_upload_boot_writes.py (static, real sources: setup() order, end() before begin() in the remount, the flag set right after the upload's LittleFS.end() and cleared on the error path, every LittleFS.begin() in helperStuff.ino guarded): origin/dev 3/5..5/5 FAILED (5 failures), fix OK 5/5. It does NOT reproduce the loss itself (the Updater and the framework mount do not run on the host); AC1 needs 09: start the FS upload on the first HTTP 200 after the app boot with telnet capturing, before and after the fix. Also green: test_fs_update_keep (38; the flag reset sits after _dropKeptFiles() so its error-path check holds), test_reboot_reason, test_reboot_log_time_not_set, test_reboot_listener_and_reason, test_crashlog_coredump, test_webserver_listener_retry, test_uptime_boot_clock, test_health_no_fs_flash_ops, test_sat_pid_persist, test_sat_preset_persist, test_static_integrity, test_ino_prototype_hoisting, test_evaluate; evaluate.py --quick 85 PASS 0 FAIL. Not covered: other setup() FS users after startWebserver() (diagRecorderBootResume, initS0Count, initSensors, initSAT) open files without begin(), so during an upload they find LittleFS unmounted and skip; initSAT is already guarded (TASK-1771). Uncommitted, no bump: build slot via the coordinator.

Correction to the previous note: on origin/dev all 5 tests of test_fs_upload_boot_writes FAIL (5 of 5); on the fix 5 of 5 pass.

2026-11-13 window coverage (coordinator question): the loop-task FS refresh loopLittleFSStatus() runs only if !isFlashing() (OTGW-firmware.ino:1089); isFlashing() = bESPactive || bPICactive (OTGW-firmware.h:1071-1072), and bESPactive is set at the upload start (OTGW-ModUpdateServer-esp32.h:356, before LittleFS.end() at :387) and cleared only after the remount and restore (:507 end, :585 abort). So the loop task is out of the whole LittleFS.end()..remount window already. The gap was in callers outside the loop: setup() (the reboot writers, now before startWebserver()), checklittlefshash() on a telnet connect and getFilesystemHash() on async_tcp, now guarded by bFsImageWriting. After the rebase onto TASK-1899 (95: refreshLittleFSFileFlags() next to loopLittleFSStatus()), check that the new refresh sits behind the same isFlashing() gate or the flag. The gate test_fs_upload_boot_writes is static; the proof is the bench: 09 repro on the old build and on the fix, at least 3 tries each (race).

2026-11-14: rebased (patch + reset + apply --3way, then ff) onto origin/dev 491aaeb7f, bumped beta.231. 95's TASK-1899 refreshLittleFSFileFlags() has one caller, loopLittleFSStatus() (helperStuff.ino:238), which is behind if (!isFlashing()) at OTGW-firmware.ino:1089: covered. setupFSexplorer()'s LittleFS.begin() runs in setup() before startWebserver(): no listener, no upload, no gap. Rerun on the rebased tree: test_fs_upload_boot_writes OK, fs_update_keep, rest_fs_flags_loop, rest_no_littlefs_on_async_tcp, health_no_fs_flash_ops, reboot tests, crashlog, uptime_boot_clock, sat_pid/preset_persist, hoisting, static_integrity, webserver_listener_retry all rc=0; evaluate.py --quick 85 PASS 0 FAIL.
<!-- SECTION:NOTES:END -->
