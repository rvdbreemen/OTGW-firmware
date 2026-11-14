---
id: TASK-1789
title: >-
  Boot task watchdog in setup() while updateRebootLog() writes LittleFS
  (gruijter coredump, alpha.532)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T22:34:00Z'
updated_date: '2026-11-14T18:32:00Z'
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
- [ ] #1 Reproduce or rule out on current code: boot timing of updateRebootLog() on a near-full or fragmented LittleFS (lfs_alloc_scan cost) on a bench unit, TWDT timeout in force
- [ ] #2 If reproducible: a fix, red on the old code and green on the fix
- [ ] #3 gruijter's unit: no newer dump after the stale one is cleared
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Research (2026-11-14, read-only, no build): ruled out on the current code, structurally. (1) alpha.532 (689cb05, the dump's build): initWatchDog() (subscribes loopTask, 30 s, OTGW-Core.ino) at OTGW-firmware.ino:522 and startWebserver() (starts async_tcp, which subscribes itself, CONFIG_ASYNC_TCP_USE_WDT) at :502 both ran BEFORE updateRebootLog() at :526: that is why the panic names exactly 'loopTask (CPU 1), async_tcp (CPU 1)'. (2) Current origin/dev: updateRebootLog() at :547 runs before startWebserver() (:549) and before initWatchDog() (:570), so during the reboot-log write neither loopTask nor async_tcp is watched. (3) Until initWatchDog() only the default IDF TWDT runs (pinned libs esp32s3/qio_qspi/include/sdkconfig.h:1090-1094: 5 s, panic, idle check CPU0 only, no CPU1 idle check); Arduino's loop WDT is off by default (cores/esp32/main.cpp:111 loopTaskWDTEnabled = false). loopTask runs on CPU1, so a slow LittleFS operation there cannot starve IDLE0; the only route would be flash erase/write cache stalls of the other core, tens of ms per 4 KB sector with IDLE0 time in between, far from 5 s. Not done: a timing measurement of lfs_alloc_scan on a near-full image (not needed for the verdict above, and it would need a bench unit with a deliberately filled FS). AC1 answered as 'ruled out on current code' (the move of updateRebootLog() ahead of the watchdog init removed the window); AC2 not applicable; AC3 (gruijter's unit: no newer dump after the stale one is cleared) is a field check. No gate added on purpose: pinning updateRebootLog() before initWatchDog() would block a future move of the watchdog init earlier in setup(), which may be wanted for boot hangs.
<!-- SECTION:NOTES:END -->
