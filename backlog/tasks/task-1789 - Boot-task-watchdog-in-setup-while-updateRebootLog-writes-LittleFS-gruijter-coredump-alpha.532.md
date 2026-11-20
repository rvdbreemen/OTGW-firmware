---
id: TASK-1789
title: >-
  Boot task watchdog in setup() while updateRebootLog() writes LittleFS
  (gruijter coredump, alpha.532)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T22:34:00Z'
updated_date: '2026-11-20T01:22:00Z'
labels:
  - crash
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 882000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coredump coredump-20261110-100556.bin from gruijter's capture upload (20261110-100556, unit on beta.167). The dump header app SHA 661d01720 matches the ELF of 2.0.0-alpha.532+689cb05 (2026-10-29), not beta.167: the coredump partition kept a dump from an older build across OTA. Decoded with that ELF (esp-coredump 1.18.1): Panic reason 'Task watchdog got triggered: loopTask (CPU 1), async_tcp (CPU 1)'. loopTask backtrace: setup() OTGW-firmware.ino:526 -> updateRebootLog() helperStuff.ino:362 -> File::close -> fclose -> lfs_file_write -> lfs_file_outline/relocate -> lfs_alloc -> lfs_alloc_scan -> lfs_fs_traverse_ -> esp_flash_read -> spi_flash_disable_interrupts_caches_and_other_cpu (cache_utils.c:176). ipc0/ipc1 idle in ipc_task with 348/340 B free: NOT the TASK-1763 IPC stack class. The call is still in setup() today (OTGW-firmware.ino:549). Files: OTGW-build-archive/coredumps/gruijter-alpha532-twdt-setup/ (dump + decode, local, not in git). Unknown: whether it still happens on current code; one stale dump only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce or rule out on current code: boot timing of updateRebootLog() on a near-full or fragmented LittleFS (lfs_alloc_scan cost) on a bench unit, TWDT timeout in force
- [x] #2 If reproducible: a fix, red on the old code and green on the fix
- [x] #3 No setup()-time TWDT in the field (gruijter's unit; reworded 2026-11-20, Robert DM 1573145820885422121 'A')
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research (2026-11-14, read-only, no build): ruled out on the current code, structurally. (1) alpha.532 (689cb05, the dump's build): initWatchDog() (subscribes loopTask, 30 s, OTGW-Core.ino) at OTGW-firmware.ino:522 and startWebserver() (starts async_tcp, which subscribes itself, CONFIG_ASYNC_TCP_USE_WDT) at :502 both ran BEFORE updateRebootLog() at :526: that is why the panic names exactly 'loopTask (CPU 1), async_tcp (CPU 1)'. (2) Current origin/dev: updateRebootLog() at :547 runs before startWebserver() (:549) and before initWatchDog() (:570), so during the reboot-log write neither loopTask nor async_tcp is watched. (3) Until initWatchDog() only the default IDF TWDT runs (pinned libs esp32s3/qio_qspi/include/sdkconfig.h:1090-1094: 5 s, panic, idle check CPU0 only, no CPU1 idle check); Arduino's loop WDT is off by default (cores/esp32/main.cpp:111 loopTaskWDTEnabled = false). loopTask runs on CPU1, so a slow LittleFS operation there cannot starve IDLE0; the only route would be flash erase/write cache stalls of the other core, tens of ms per 4 KB sector with IDLE0 time in between, far from 5 s. Not done: a timing measurement of lfs_alloc_scan on a near-full image (not needed for the verdict above, and it would need a bench unit with a deliberately filled FS). AC1 answered as 'ruled out on current code' (the move of updateRebootLog() ahead of the watchdog init removed the window); AC2 not applicable; AC3 (gruijter's unit: no newer dump after the stale one is cleared) is a field check. No gate added on purpose: pinning updateRebootLog() before initWatchDog() would block a future move of the watchdog init earlier in setup(), which may be wanted for boot hangs.

In Review (coordinator, 2026-11-14): AC1 ruled out on the current code (citations above: the reboot log is written before initWatchDog() and startWebserver(), so neither loopTask nor async_tcp is watched; the default IDF TWDT only checks IDLE0); AC2 not applicable (nothing to fix on the current code); AC3 open in the field: gruijter's unit shows no newer dump after the stale alpha.532 one is cleared.

2026-11-20 (otgw-dev, coordinator triage): AC1 ticked as 'ruled out on current code' (citations in the 2026-11-14 note: updateRebootLog() runs before initWatchDog() and startWebserver(), so neither loopTask nor async_tcp is watched then; the default IDF TWDT only checks IDLE0). AC2 ticked as not applicable: nothing reproduces, so there is no fix to prove red/green. AC3 as written is NOT met: gruijter's reboot log (OTGW-field-data/gruijter/2026-11-17_2.0.0-beta.356+cf29dd1, crashlog.log) has newer TWDT dumps, but none at setup() time: 2026-11-12 14:06/17:58 and 2026-11-13 04:00 async_tcp (TASK-1899, Done), 2026-11-13 23:56 and 2026-11-18 01:47:55 loopTask in loopLittleFSStatus -> lfs_fs_size (TASK-1995, moved off the loop task in 41a9e44ed, beta.370). About 6 boots/OTAs 2026-11-12..18 show no boot-time TWDT. The coordinator puts rewording AC3 ('no newer setup()-time dump') to Robert.

2026-11-20 AC3 reworded (Robert, DM 1573145820885422121, 'A'). Old wording: 'gruijter's unit: no newer dump after the stale one is cleared'. New: 'No setup()-time TWDT in the field'. Evidence: gruijter's reboot log in OTGW-field-data/gruijter/2026-11-17_2.0.0-beta.356+cf29dd1/20261117-212615-upload.zip (crashlog.log, read 2026-11-17 21:26 and again 2026-11-18 01:47) covers 2026-11-12 12:11 to 2026-11-18 01:47. Every TWDT entry there is a running-task dump, none setup()-time. async_tcp (TASK-1899, Done): 2026-11-12 14:06:56, 2026-11-12 17:58:48, 2026-11-13 04:00:54. loopTask in loopLittleFSStatus -> lfs_fs_size (TASK-1995, decoded there): 2026-11-13 23:56:18 (same bt shape), 2026-11-18 01:47:55. The other entries are OTA/software resets (2026-11-12 12:11/12:12/21:30/21:37, 11-14 19:49/19:50, 11-15 21:28/21:29, 11-16 21:45/21:46) plus 2 'Other watchdog (7)' with time not set, which carry no dump. Each of those boots passed updateRebootLog() without a TWDT. NOT verified: what the two time-not-set 'Other watchdog' boots were (no dump, no time).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: ruled out on current code. updateRebootLog() runs before initWatchDog() and startWebserver(), so no watched task can starve then; the default IDF TWDT checks only IDLE0 (citations 2026-11-14). AC2: not applicable, nothing to fix. AC3 (reworded per Robert DM 1573145820885422121): gruijter's beta.356 reboot log 2026-11-12..18 shows no setup()-time TWDT. Its TWDT dumps are async_tcp (TASK-1899) and loopTask lfs_fs_size (TASK-1995), tracked there. Not verified: the two time-not-set 'Other watchdog (7)' entries, which have no dump.
<!-- SECTION:FINAL_SUMMARY:END -->
