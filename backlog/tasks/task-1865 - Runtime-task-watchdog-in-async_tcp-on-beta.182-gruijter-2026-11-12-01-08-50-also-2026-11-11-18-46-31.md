---
id: TASK-1865
title: >-
  Runtime task watchdog in async_tcp on beta.182 (gruijter 2026-11-12 01:08:50,
  also 2026-11-11 18:46:31)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T11:51:00Z'
updated_date: '2026-11-12T12:17:00Z'
labels:
  - bug
  - crash
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 941000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's overnight capture (#alpha-testing msg https://discord.com/channels/812969634638725140/1514720723980259460/1570395420667023420, upload 20261111-213113-upload.zip; analysis by the coordinator's /analyze-capture agent). At about 01:08 the gateway ESP rebooted on a Task watchdog crash in async_tcp (reboot #96, beta.182); same pattern on 2026-11-11 18:46:31 on the previous build. Core dump: pc=0x4037627a, elf=ebfbbaf5; the zip holds coredump-20261111-213113.bin with a matching app SHA. Side effect seen: after the reboot SAT ran one tick before the first room temperature (curve only, CS=30.1 + CH enable for 22 s, burner did not light). Different from TASK-1763 (ipc1 boot panic) and TASK-1789 (setup TWDT in updateRebootLog). A crash, so no needs-maintainer-ok.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The core dump coredump-20261111-213113.bin is decoded with the beta.182 .elf (app SHA ebfbbaf5) and the backtrace of the async_tcp watchdog is recorded
- [ ] #2 The backtrace is compared with the current dev code: the cause is named with file:line, or shown to be fixed already by a later commit
- [ ] #3 Fix with a reproduction red on the old code and green on the fix, or close as fixed with the evidence of the commit that fixed it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Claimed by otgw-firmware-95 (2026-11-12, coordinator assignment). Step 1, no build: decode the core dump from gruijter's upload against the beta.182+c9f37bd .elf, name the stuck task and code path, compare with current dev; report before any fix.

Step 1 decode (95, 2026-11-12, no build). Sources: gruijter's 20261111-213113-upload.zip (Discord msg 1570395420667023420), coredump-20261111-213113.bin (33,188 B), decoded with esp-coredump 1.18.1 + xtensa-esp32s3-elf-gdb against OTGW-build-archive/2.0.0-beta.182+c9f37bd .elf (app SHA matches). Untrusted files kept in the 95 scratchpad (t1865/), not in the repo.

CORRECTION of the task title/description: the watchdog did NOT fire on async_tcp. Panic reason: 'Task watchdog got triggered ... - loopTask (CPU 1)'. async_tcp is only the 'crashed task' because it was the task running on CPU 1 when the TWDT interrupt was handled. TWDT timeout in this firmware is 30 s (OTGW-Core.ino:1570). Timeline from the transcript: last telnet line 01:07:51.69 (loopTask, a normal MQTT publish), then a 42 s telnet gap, reboot banner 'Boot : Task watchdog', telnet back at 01:08:33; so the panic was about 01:08:21, not 01:08:50.

Facts from the dumped TCBs and stacks (valid): async_tcp prio 10, xCoreID 1, running, backtrace: AsyncTCP _recv -> processAPI -> handleHealth -> sendHealth (restAPI.ino:3582) -> platformFSInfo (platform_esp32.h:167) -> LittleFS.totalBytes -> esp_littlefs_info -> lfs_fs_size -> lfs_fs_traverse -> esp_partition_read -> esp_flash_read -> spi_flash_disable_interrupts_caches_and_other_cpu at cache_utils.c:176 (IDF 9bb7aa84fe: the busy-wait 'while (!s_flash_op_can_start)' after vTaskSuspendAll and esp_ipc_call_nonblocking(other cpu = 0, spi_flash_op_block_func)). async_tcp holds 2 mutexes (uxMutexesHeld 2, likely the littlefs and flash locks). loopTask prio 1, xCoreID 1, on pxReadyTasksLists[1] (READY, not blocked; xEventListItem in no list), uxMutexesHeld 1 (it had just taken the espMqttClient mutex in MqttClient::loop, MqttClient.cpp:270). ipc0 (CPU 0) is on xSuspendedTaskList (blocked in ulTaskGenericNotifyTake, portMAX) while its TCB says ucNotifyState = 2 (notified) and ulNotifiedValue = 1. IDLE0 and IDLE1 sit in esp_cpu_wait_for_intr.

Reading: async_tcp (prio 10) spun on CPU 1 with the scheduler suspended, waiting for ipc0 on CPU 0 to run spi_flash_op_block_func; ipc0 never woke; loopTask (prio 1, same core) could not run for 30 s -> TWDT names loopTask. The anomaly is ipc0: 'notified' and still blocked. FreeRTOS only leaves a task blocked after a give when the target was not in the waiting state at that moment, so ipc0's notify state was already non-waiting when CPU 1's IPC notify arrived: a lost wakeup.

Hypothesis for the lost wakeup (strong, not proven by this dump): memory layout from the TCBs: ipc0 stack 0x3fcbbc74-0x3fcbc070, ipc0 TCB 0x3fcbc0b8-0x3fcbc218 (352 B) with ulNotifiedValue at 0x3fcbc210 and ucNotifyState at 0x3fcbc214 (its last 8 bytes), ipc1 stack starts at 0x3fcbc22c, 24 B above. An ipc1 stack overflow writes downward into ipc0's TCB tail, exactly the notify fields. ADR-225 / TASK-1763 already measured ipc1 overflowing its 1024 B stack by 52 B on .61 (and an ipc0 panic on .63); the Arduino S3 libs are built with CONFIG_ESP_IPC_TASK_STACK_SIZE=1024 (sdkconfig), IDF default 1280. If that holds, this is a second failure mode of the ADR-225 root cause: not a stack-overflow panic but a corrupted ipc0 notify state that later deadlocks the first cross-core flash operation. Not proven: the ipc1 stack bottom and the heap gap are not in the dump.

NOT valid evidence (correction of my own first reading): globals such as xTickCount, uxSchedulerSuspended, xPendingReadyList and s_flash_op_can_start are not in this core dump; gdb returns their ELF initial values (xTickCount = 0 proves it). Only TCBs and task stacks were dumped.

Second crash (2026-11-11 18:46:31): its dump was not found. The other local gruijter dump (coredump-20261110-211847.bin, upload 20261110) matches no archived combo .elf from beta.160-179 (app SHA 661d01720), so it cannot be decoded. One decoded dump only.

Separate observation, not the cause: every /api/v2/health call runs LittleFS.totalBytes() on async_tcp, a full lfs_fs_size traversal of the filesystem metadata with one cross-core flash read per block (a health call after the reboot took 181 ms). Each such read is a cross-core IPC flash operation, so health polling (every 15 s from the web UI) is the most frequent trigger of the path that hung. Current dev unchanged here: restAPI.ino:3587 still calls platformFSInfo in the health path; espMqttClient unchanged since c9f37bd (0 diff lines).

Candidate directions (for the coordinator / Robert, nothing done): (A) ADR-225: rebuild the S3 framework libs with CONFIG_ESP_IPC_TASK_STACK_SIZE >= 1280 (root cause if the hypothesis holds; Proposed, build-heavy); (B) cache the LittleFS total/used bytes (refresh on write, or once a minute on the loop task) so /health does no flash traversal on async_tcp (removes the most frequent trigger, cheap, independent of A); (C) test the hypothesis: an ipc1 stack-watermark probe (uxTaskGetStackHighWaterMark on both ipc tasks in device/info) on the bench under the TASK-1763 reboot/interrupt load.
<!-- SECTION:NOTES:END -->
