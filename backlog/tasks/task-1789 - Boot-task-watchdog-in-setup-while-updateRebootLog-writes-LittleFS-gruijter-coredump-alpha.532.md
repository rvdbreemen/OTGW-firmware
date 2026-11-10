---
id: TASK-1789
title: >-
  Boot task watchdog in setup() while updateRebootLog() writes LittleFS
  (gruijter coredump, alpha.532)
status: To Do
assignee: []
created_date: '2026-11-10T22:34:00Z'
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
