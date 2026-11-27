---
id: TASK-3142
title: >-
  TWDT in setup(): the flash-op IPC request to CPU 0 was never executed (initSAT
  -> satLoadEnergyState -> LittleFS open ->
  spi_flash_disable_interrupts_caches_and_other_cpu), first boot after a
  safeboot OTA, beta.432
status: To Do
assignee: []
created_date: '2026-11-27T21:11:00Z'
updated_date: '2026-11-27T21:17:00Z'
labels:
  - crash
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1181000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter's capture holds a core dump of 2026-11-25 20:46:45 'Task watchdog', loopTask, elf 98094d52 = 2.0.0-beta.432+43ac963 (the ELF is in OTGW-worktree-leftovers/wt-d3-doc-beta.432-build). Full decode: OTGW-field-data/_comms/decode_gruijter_twdt_2026-11-25.md and _gdb.txt. Chain of the 16 frames of the dump: spi_flash_disable_interrupts_caches_and_other_cpu (cache_utils.c:176) <- cache_disable <- spi1_start <- spiflash_start_default <- esp_flash_read <- esp_partition_read <- littlefs_esp_part_read <- lfs_bd_read <- lfs_dir_getslice <- lfs_dir_get <- lfs_file_opencfg_ <- lfs_file_opencfg <- vfs_littlefs_open <- esp_vfs_open <- _fopen_r <- fopen; the full stack continues VFSImpl::open <- fs::FS::open <- satLoadEnergyState (SATcontrol.ino:4284) <- initSAT (:6500) <- setup (OTGW-firmware.ino:633) <- loopTask. loopTask (CPU 1, 30 s TWDT since initWatchDog at :581) spun 30 s in the busy-wait for CPU 0 to start its part of the flash stall. The three statics/states of the dump: s_flash_op_can_start = false, s_flash_op_complete = false (the stall function was never started on CPU 0), and ipc0 and ipc1 both idle in ulTaskGenericNotifyTake(portMAX_DELAY) (ipc_task, src/OTGW-firmware/esp_ipc_stack.c:87, our own copy from ADR-225) with IDLE0 in wait-for-interrupt: CPU 0 was idle, the IPC request was lost or ignored, not starved. Not the bad_alloc / AsyncTCP class (TASK-3025, TASK-3119, ADR-301): async_tcp is idle, no allocation in the stack. Same family: TASK-1870 (ADR-225 IPC stack), TASK-1763 (boot panic in ipc1), TASK-1789 (boot TWDT in setup), TASK-1899 (cross-core flash read). beta.474 has no change to esp_ipc_stack.c since 43ac963 and no change to the order initWatchDog -> initSAT: no fix. Single occurrence on a unit that was on the first boot after a safeboot OTA; the same unit shows no second Task watchdog in its reboot log through 27 Nov 21:58 (about 30 h). NOT proven: why the IPC request was not acted on (the IPC statics are optimized out in the ELF). Next step: compare src/OTGW-firmware/esp_ipc_stack.c with the IPC code of the linked framework (esp_ipc_call_nonblocking, the notify and s_no_block_func_and_arg_are_ready order); if identical it is an upstream IDF race, if different ADR-225 is the cause.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 esp_ipc_stack.c is compared with the IPC code of the linked framework build and the difference (or the absence of one) is written in the task notes
- [ ] #2 If our copy loses a wake-up the path is fixed with a host test red on the old code; if it is identical the race is reported upstream and a mitigation (a bounded retry or a log of the IPC state) is decided
- [ ] #3 A second dump, if one appears, is decoded against this one (frames, ipc0 state, can_start flag)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 reconciled with otgw-dev: the timeout is the firmware's own 30 s (initWatchDog, esp_task_wdt timeout_ms = 30000, trigger_panic = true, OTGW-Core.ino:1639 on 43ac963), not the IDF default of 5 s: loopTask sat 30 s inside the flash-cache-disable call (cache_utils.c:176). Evidence for the cause is the gdb decode of all tasks (poc-dev): CPU 0 idle (IDLE0 in wait-for-interrupt), ipc0 and ipc1 idle in ulTaskGenericNotifyTake, s_flash_op_can_start and s_flash_op_complete both false: the IPC request was never executed. The caller of fopen is not in the 16-frame summary; the full gdb stack names it: satLoadEnergyState (SATcontrol.ino:4284) <- initSAT (:6500) <- setup (OTGW-firmware.ino:633). otgw-dev did not run gdb himself and has not verified the task states independently. Decode: OTGW-field-data/_comms/decode_gruijter_twdt_2026-11-25.md
<!-- SECTION:NOTES:END -->
