---
id: TASK-1208
title: Task-watchdog reset (rst=6) under the 8-worker static-file storm on OTGW32
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-04 09:23'
updated_date: '2026-10-05 14:31'
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
- [ ] #1 Reproduced: the storm (or the BLE low-heap condition) produces a task-watchdog reset on the bench, with the crash or backtrace captured right after the reset and before any reflash
- [ ] #2 Root cause identified: which task missed its watchdog feed and why
- [ ] #3 Fixed or explicitly deferred with the maintainer, with old-vs-fix evidence for a fix
- [ ] #4 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-05 static pre-work (no bench; the TASK-1036 soak holds it):
- Why no crash data: /api/v2/device/crashlog reads only /reboot_log.txt (helperStuff.ino readLatestCrashLog). The TWDT is armed with trigger_panic=true (OTGW-Core.ino initWatchDog, both branches), and sdkconfig has CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=1 (ELF, CRC32). The partition tables carry a 64 KB coredump partition at 0x3F0000. So each TWDT panic writes an ELF core with the stalled task and its backtrace, which nothing reads. LittleFS ends at 0x3F0000, so app-only and --fs flashes leave the core intact.
- Capture recipe for AC#1 (host side, no firmware change): esptool read_flash 0x3F0000 0x10000 core.bin over USB right after the reset, then esp-coredump info_corefile -t raw -c core.bin <matching .elf>. Tooling present: esp-coredump 1.16.0 in the pio penv; tool-xtensa-esp-elf-gdb 12.1 installed today (xtensa-esp32s3-elf-gdb.exe). ELFs kept in %LOCALAPPDATA%/OTGW-capture/elf/ (combo alpha.416+9f64919) and variants-CD/firmware_CD.elf. The core records the ELF hash, so the ELF must match.
- Which task: only the Arduino loop task is subscribed. AsyncTCP forces CONFIG_ASYNC_TCP_USE_WDT 0 (AsyncTCP.cpp:18-19). initWatchDog reconfigures to 30 s with idle_core_mask=0, and IDF 5.5 esp_task_wdt_reconfigure unsubscribes the old idle tasks (task_wdt.c:582-590). So rst=6 means the loop task did not feed for at least 30 s. The 5.1 s CDC gap is the tail of that block, not a 5 s timeout. Lead to confirm with the core: the loop task blocked inside a call that waits on lwIP/AsyncTCP under memory exhaustion (TASK-1162 window).
- Order after the soak: HTTP readout of TASK-1036 first; then read the coredump partition BEFORE any storm run (it may hold an earlier panic); re-read it after every rst=6.
- Option for the maintainer (not done, new API surface): show the coredump summary (esp_core_dump_get_summary: task name, PC, backtrace) in /api/v2/device/crashlog so testers without USB can report it.
<!-- SECTION:NOTES:END -->
