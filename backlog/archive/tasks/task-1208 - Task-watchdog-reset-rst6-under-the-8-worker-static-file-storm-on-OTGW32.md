---
id: TASK-1208
title: Task-watchdog reset (rst=6) under the 8-worker static-file storm on OTGW32
status: Done
assignee:
  - '@claude'
created_date: '2026-10-04 09:23'
updated_date: '2026-10-06 05:05'
labels:
  - esp32
  - watchdog
  - reliability
dependencies: []
priority: medium
ordinal: 330000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1162 (run 2026-10-04). In the TASK-1162 diagnostic run (T1162diag v2 on alpha.404+b6e900a, refresh_storm.py --workers 2,4,6,8 --seed 1124, 33% client aborts after 1024 body bytes), the second deaf window (t=327-368 s) was a reboot. The banner went from boots=32 to 33 with reset reason 6 (task watchdog). The device reboot_log.txt confirms: '2026-10-04 08:51:57 - reboot cause: Task watchdog (6)'. The CDC stream gapped 5.1 s before the reset.
The same log has two earlier unexplained resets on the same bench, during the TASK-1199 BLE measurements on alpha.405 (no PSRAM, BLE on, REST answering 503 'low heap'): 08:00:22 Task watchdog (6) and 08:11:50 Exception/panic (4).
No crash data was kept: /api/v2/device/crashlog reports available:false. The board has been reflashed since. Which task starved the watchdog is unknown.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced: the storm (or the BLE low-heap condition) produces a task-watchdog reset on the bench, with the crash or backtrace captured right after the reset and before any reflash
- [x] #2 Root cause identified: which task missed its watchdog feed and why
- [x] #3 Fixed or explicitly deferred with the maintainer, with old-vs-fix evidence for a fix
- [x] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-05 static pre-work (no bench; the TASK-1036 soak holds it):
- Why no crash data: /api/v2/device/crashlog reads only /reboot_log.txt (helperStuff.ino readLatestCrashLog). The TWDT is armed with trigger_panic=true (OTGW-Core.ino initWatchDog, both branches), and sdkconfig has CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=1 (ELF, CRC32). The partition tables carry a 64 KB coredump partition at 0x3F0000. So each TWDT panic writes an ELF core with the stalled task and its backtrace, which nothing reads. LittleFS ends at 0x3F0000, so app-only and --fs flashes leave the core intact.
- Capture recipe for AC#1 (host side, no firmware change): esptool read_flash 0x3F0000 0x10000 core.bin over USB right after the reset, then esp-coredump info_corefile -t raw -c core.bin <matching .elf>. Tooling present: esp-coredump 1.16.0 in the pio penv; tool-xtensa-esp-elf-gdb 12.1 installed today (xtensa-esp32s3-elf-gdb.exe). ELFs kept in %LOCALAPPDATA%/OTGW-capture/elf/ (combo alpha.416+9f64919) and variants-CD/firmware_CD.elf. The core records the ELF hash, so the ELF must match.
- Which task: only the Arduino loop task is subscribed. AsyncTCP forces CONFIG_ASYNC_TCP_USE_WDT 0 (AsyncTCP.cpp:18-19). initWatchDog reconfigures to 30 s with idle_core_mask=0, and IDF 5.5 esp_task_wdt_reconfigure unsubscribes the old idle tasks (task_wdt.c:582-590). So rst=6 means the loop task did not feed for at least 30 s. The 5.1 s CDC gap is the tail of that block, not a 5 s timeout. Lead to confirm with the core: the loop task blocked inside a call that waits on lwIP/AsyncTCP under memory exhaustion (TASK-1162 window).
- Order after the soak: HTTP readout of TASK-1036 first; then read the coredump partition BEFORE any storm run (it may hold an earlier panic); re-read it after every rst=6.
- Option for the maintainer (not done, new API surface): show the coredump summary (esp_core_dump_get_summary: task name, PC, backtrace) in /api/v2/device/crashlog so testers without USB can report it.

2026-10-06 01:05: ROOT CAUSE from the coredump of the original reset. The coredump partition (0x3F0000) read over USB right after the TASK-1036 soak still held a core whose app ELF SHA256 6ccf687c5 matches exactly the T1162diag-v2 alpha.404+b6e900a image that took the 2026-10-04 08:51 rst=6 (no other kept image matches; ELF kept from wt-1162/build). esp-coredump 1.16 + xtensa-esp32s3-elf-gdb, --gdb-timeout-sec 30: 'Task watchdog got triggered ... loopTask (CPU 1)'. loopTask backtrace: loop() OTGW-firmware.ino:1088 -> doBackgroundTasks() :987 -> handleMQTT() MQTTstuff.ino:1101 -> MqttClient::loop :271 -> _checkOutbox :355 -> _sendPacket :371 -> ClientSync::write (575 B) -> NetworkClient::write -> esp_vfs_select -> lwip_select -> sys_arch_sem_wait(timeout=1000). All other tasks idle or waiting normally (tiT in mbox_fetch, picSerial in its 2 ms delay, async_tcp waiting). So the loop task sat in a blocking MQTT socket write under the storm's memory exhaustion for over 30 s: the same defect TASK-1213 found and ADR-186 fixed (non-blocking send with MSG_DONTWAIT, alpha.415+). Artifacts: %LOCALAPPDATA%/OTGW-capture/task1208/ (core-before-storm.bin, the matching .elf, core-alpha404-T1162v2.txt). Next: storm on alpha.416 (fix) to show no rst=6; old-code storm runs (alpha.412 base) as the old side, reading the core after any reset.

2026-10-06 01:10-01:30 FIX side: three runs of the same storm (refresh_storm.py --workers 2,4,6,8 --seed 1124) on alpha.416+9f64919, which carries ADR-186 (ClientSync::write sends with MSG_DONTWAIT and never calls the blocking NetworkClient::write on ESP32). Result: 0 of 3 rebooted (bootcount 18 before and after every run), MQTT stayed connected. The storm still drove memory down (hd_min_free_heap 580 / 568 / 256 B, hd_min_max_block down to 1268 B, loop-gap watermark 3010 ms): that is the TASK-1162 exhaustion, but the loop task no longer parks 30 s in a socket write. OLD side: the decoded coredump of the 2026-10-04 08:51 reset (alpha.404 T1162diag-v2, same storm) shows loopTask stuck in NetworkClient::write -> lwip_select. Caveat, stated honestly: the old reset rate was low (1 TWDT in the TASK-1162 storm series), so 0/3 on the fix is not statistically strong on its own; the stronger argument is structural: the blocking call in the backtrace no longer exists on the MQTT path. Further old-code storms (alpha.412 base in the TASK-1162 variant series) read the coredump after any reset. Results: %LOCALAPPDATA%/OTGW-capture/task1208/fix-a416/.

2026-10-06: the TASK-1162 variant series ran 3 more storms on old code (alpha.412 base, deaf up to 542 s per run) and 9 on alpha.412 variants: no reboot in any of the 12, so no fresh TWDT to capture. The reset is rare on old code. AC#1 evidence therefore remains the coredump of the original 2026-10-04 08:51 reset, captured before any write to the coredump partition.

2026-10-06: maintainer accepted the original-reset coredump as AC#1 evidence (no fresh repro in 12 further storms; the reset is rare).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Root cause of the task-watchdog reset (rst=6) under the static-file storm: the Arduino loop task blocked for over 30 s in a blocking MQTT socket write. Evidence: the ELF coredump of the original 2026-10-04 08:51 reset, still in the coredump partition at 0x3F0000 (it survives app-only and filesystem flashes) and matched to its build by app ELF SHA256 6ccf687c5 (alpha.404 T1162diag-v2). Decoded with esp-coredump + xtensa-esp32s3-elf-gdb: 'loopTask (CPU 1)' did not feed the TWDT; backtrace loop -> doBackgroundTasks -> handleMQTT -> MqttClient::loop -> _checkOutbox -> _sendPacket -> ClientSync::write -> NetworkClient::write -> lwip_select. Only the loop task is TWDT-subscribed (30 s, idle tasks unsubscribed; AsyncTCP forces its WDT off). Fix: ADR-186 (TASK-1213, alpha.415+), ClientSync::write sends with MSG_DONTWAIT and never blocks. Fix evidence: 3 storms on alpha.416, 0 reboots, MQTT connected, while memory still fell to 256 B (the separate TASK-1162 exhaustion). AC#1 rests on the original reset's coredump, not a fresh reproduction: 12 further old-code storms did not reset (maintainer accepted this). Builds and evaluate: the alpha.416 run (three targets SUCCESS, evaluate 71/0). Side finding: /api/v2/device/crashlog reads only /reboot_log.txt; nothing reads the coredump. Capture recipe and tooling are in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
