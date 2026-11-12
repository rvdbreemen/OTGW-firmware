---
id: TASK-1865
title: >-
  loopTask starved into the task watchdog on beta.182: async_tcp stuck in a
  cross-core flash read from /api/v2/health (gruijter 2026-11-12 ~01:08:21, also
  2026-11-11 18:46:31)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T11:51:00Z'
updated_date: '2026-11-12T22:50:00Z'
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
- [x] #1 The core dump coredump-20261111-213113.bin is decoded with the beta.182 .elf (app SHA ebfbbaf5) and the backtrace of the async_tcp watchdog is recorded
- [x] #2 The backtrace is compared with the current dev code: the cause is named with file:line, or shown to be fixed already by a later commit
- [ ] #3 Fix with a reproduction red on the old code and green on the fix, or close as fixed with the evidence of the commit that fixed it
- [x] #4 Bench by the bench-tester on .61: the TASK-1763 boot/interrupt load plus /api/v2/health polling, hd_ipc0_stack_min / hd_ipc1_stack_min reported per run (C), no task watchdog; result decides whether ADR-225 (A) goes to Robert
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

Title changed (coordinator, 2026-11-12). Old title: 'Runtime task watchdog in async_tcp on beta.182 (gruijter 2026-11-12 01:08:50, also 2026-11-11 18:46:31)'. The decode showed the watchdog named loopTask, starved by async_tcp on the same core.

Decision (coordinator, 2026-11-12): B + C in one build now (beta.217); A (ADR-225 lib rebuild) only after C confirms, its acceptance then goes to Robert. B: /api/v2/health does no LittleFS flash operation on async_tcp; the mount check (60 s, platformFSInfo) and the write probe (5 min, updateLittleFSStatus('/.health'), TASK-1205) moved to loopLittleFSStatus() (helperStuff.ino), called from doBackgroundTasks() when not flashing; sendHealth() only reports LittleFSmounted (LittleFSinfo's sizes are read nowhere, so the old 2 traversals per poll only refreshed that bool). Detection of an unmounted filesystem now takes up to 60 s instead of the next poll. C: device/info hd_ipc0_stack_min / hd_ipc1_stack_min from platformIpcStackHighWater() (TASK-1763 shim), frozen in the DeviceInfoSnap; the hd_* group is not in docs/api (existing undocumented diagnostics), so no API doc change.

Evidence B+C: tests/test_health_no_fs_flash_ops.py (4 tests: sendHealth body has no platformFSInfo/updateLittleFSStatus; doBackgroundTasks calls loopLittleFSStatus outside flashing; the real loopLittleFSStatus with the real safeTimers.h over 16 simulated minutes does 3 write probes and 13-16 mount checks, against 128 traversals from one tab's health polls before; device/info snapshot + emit of both ipc fields). Fix 4/4 OK; old code (origin/dev restAPI/helperStuff/OTGW-firmware.ino) 4/4 FAIL. Sweep: 51 tests slicing restAPI/helperStuff/OTGW-firmware.ino/sendHealth/device-info green (foreground, one at a time), test_dhw_water_meter 407 s and test_sat_lowload_contract 483 s green, evaluate --quick 92/0 failed/0 warnings.

AC1 ticked: decoded against the beta.182 .elf (SHA match), backtraces of all 17 tasks in the 95 scratchpad t1865/decode.txt. AC2 ticked: compared with dev (restAPI.ino:3587 still had platformFSInfo in sendHealth; espMqttClient unchanged): the starvation path is named with file:line (restAPI.ino sendHealth -> platform_esp32.h:167 -> lfs_fs_size -> cache_utils.c:176, loopTask ready on the same core); the root of the lost ipc0 wakeup is a hypothesis (ADR-225 ipc1 overflow into ipc0's TCB tail) that C measures. AC3 stays open until the bench (new AC4): B removes the most frequent trigger from async_tcp, it is not proven to remove the hang.

Bench (otgw-bench-tester 09, 2026-11-12, beta.217+84c3514 on .61, fwversion confirmed, settings changed: none): PASS on the pass criteria. 38 boots (32 quick with 30 s dwell, 6 with 10 min uptime; more than TASK-1763's 36), bootcount 483 -> 521, +1 per boot, all Software reset; 0 TWDT/panic/Guru/abort/Backtrace lines in telnet (136 loop stalls, max 689 ms), no new crashlog entry, lowest free heap 10912 B. Load alongside: Chrome with 2 v2-UI tabs across reboots, Firefox as constant WS client, telnet, device/info every 5 s, /health every 15 s.

B (bench): /health 325 samples, littlefsMounted true in all, 0 non-200 outside reboot gaps. First 60 s after boot n=86 median 99 ms p95 314 max 1282; steady state n=227 median 98 p95 256 max 659 (p95 219 / max 443 without the 6 s before a reboot request). beta.182 median after a reboot was ~181 ms, so about half now. The 5-min write probe logs nothing on success and was not observed directly; at boot+300 s /health stayed 59-179 ms, no stall above 249 ms.

C (bench): 74 ipc readings, never -1. ipc1 min free stack 80 B in every reading (baseline, 32 quick boots, 6 x 10 min boots at +60 s and +600 s, end at bc 521); ipc0 152-232 B, lowest 152. ipc1 constant: looks like a fixed path, not load-dependent. Reading (95): no ipc1 overflow on any of 38 boots, so the ADR-225 overflow hypothesis is NOT supported by this data, and not ruled out either (the beta.182 failure was not reproduced). 80 of 1024 B is a thin margin (TASK-1763 measured a 52 B overflow on another load), so ADR-225 (A) goes to Robert as a margin argument, not as a proven cause. AC4 ticked.

Regression (bench): v2 UI loads with 0 console errors (b217/ui61-home.png); MQTT connected with fresh sat/* and otgw32/* publishes; settings save LEDblink false/true POST 200, on disk within 3 s, GET confirms, diff 0 (518 keys). Not verified: the write probe itself; the real beta.182 pattern (lost ipc0 wakeup during a cross-core flash read) was not reproduced, so 38 clean boots do not prove it gone; no A/B against older builds (no ipc fields there); no real OT bus load (no boiler); no steady state beyond 10 min. AC3 stays open: the host test shows the flash traversal leaves async_tcp (red old / green fix), but the hang itself needs field confirmation (gruijter on 217+). Evidence: C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\b217 (run/events.log, boots.jsonl, health.csv, telnet-raw.txt, 61-ota.log, ui61-home.png).

Published (95, 2026-11-12): pushed 22d269e0e..a7676dcc8 to dev; posted 2.0.0-beta.217+a7676dc (esp32-combo flash zip, 6315083 B) in #alpha-testing, Discord message 1570494289371205743. Confirm bench by 09 on a7676dc (src identical to the benched 84c3514): fwversion exact, version.hash flipped, /health 20/20 mounted, ipc0 232->184 / ipc1 80 B, 5 min no reboot/TWDT, settings diff 0 (09 scratchpad b217b/). .elf archived as OTGW-build-archive/2.0.0-beta.217+a7676dc.zip. AC3 open until field confirmation on 217+ (gruijter). ADR-225 (A) chosen by Robert (DM 1570489845019840593) goes as a separate task.

2026-11-12 gruijter capture on beta.214+5d03c17 (before the 217 fix): two Task-watchdog resets, 14:06:20 (Reboots 99, transcript banner line 73267) and 17:58:20 (Reboots 100, line 216013); no backtrace in the capture, core dump HTTP 503, crashlog/reboot_log not in the upload; heap just before was healthy (14:05:50 51972 free / 29684 maxblk; 17:58:00 51860 / 31732), WS/MQTT drops 0. The capture cannot show whether these took the health/LittleFS path. gruijter asked to update to 218 and send /api/v2/device/crashlog + reboot_log.txt on a new reset. Source https://discord.com/channels/812969634638725140/1514720723980259460/1570542193934860348

2026-11-12 (comms): second field unit for confirming the fix: cmdr_langers (Ideal Logic 35, Wiser HubR, PIC 6.8) asked to run beta.218 with Nightly Restart off and send crashlog + reboot_log.txt on an unexpected restart (#english-support msg 1570571315977527407); see TASK-1544 note.
<!-- SECTION:NOTES:END -->
