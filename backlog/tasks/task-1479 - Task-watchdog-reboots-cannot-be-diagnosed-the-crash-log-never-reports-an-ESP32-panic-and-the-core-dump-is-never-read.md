---
id: TASK-1479
title: >-
  Task-watchdog reboots cannot be diagnosed: the crash log never reports an
  ESP32 panic and the core dump is never read
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T14:37:00Z'
updated_date: '2026-10-30T00:00:00Z'
labels:
  - bug
  - stability
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 639000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's field capture (TASK-1474, alpha.527): Task-watchdog reboot at 2026-10-29 12:56:52 (reboot #41). Telnet and the loop-driven MQTT client both went silent after 12:56:17.287, so the loop stopped; which watched task tripped is unknown (the TWDT watches the loop task AND AsyncTCP's _async_service_task, CONFIG_ASYNC_TCP_USE_WDT=1 in AsyncTCP 3.4.10 AsyncTCP.h:41-42 / AsyncTCP.cpp:321; the comment at OTGW-Core.ino:1481 saying 'loop task only' is wrong). The unit's reboot_log shows 5 earlier Task-watchdog reboots (2026-10-22, 10-23 x2, 10-25 x2), so it recurs. /api/v2/device/crashlog stays available:false: readLatestCrashLog() (helperStuff.ino ~244) only accepts the ESP8266 'ESP register contents:'/'External Reason:' lines, and platformResetRegisterDump() is a no-op on ESP32 (platform_esp32.h:527). Core dump to flash is enabled in the framework sdkconfig and partitions_otgw_esp32.csv has a 64K coredump partition at 0x3F0000, but nothing reads it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a forced loop stall > 30 s on the bench, the web UI / REST can show which task tripped and its backtrace (core dump summary or download), and the same works after a real field TWDT
- [x] #2 The crash-log endpoint reports an ESP32 panic/TWDT reboot instead of available:false
- [x] #3 The OTGW-Core.ino TWDT comment names every task subscribed to the TWDT
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Facts checked (framework-arduinoespressif32-libs esp32s3 sdkconfig.h ~1148-1156): core dump to flash ON, ELF format, CRC32; esp_core_dump_image_check/_get/_erase and esp_core_dump_get_summary/_get_panic_reason available (espcoredump esp_core_dump.h:104-172; xtensa summary: exc_task[16], exc_pc, bt[16]+depth+corrupted, app_elf_sha256). TWDT subscribers: loop task (OTGW-Core.ino:1520/1637) and AsyncTCP _async_service_task (AsyncTCP.cpp:322, CONFIG_ASYNC_TCP_USE_WDT default 1 at AsyncTCP.h:41-42, not overridden in platformio.ini); espMqttClient only with its internal task (MqttClient.cpp:330, EMC_USE_WATCHDOG) which we do not use (UseInternalTask::NO, MQTTstuff.ino:209); IDLE not watched after our reconfigure (idle_core_mask=0). ISR hook esp_task_wdt_isr_user_handler exists (esp_task_wdt.h:180).
Open-source check: no library needed; this is ESP-IDF's own core-dump API (Espressif, Apache-2.0, already linked). Offline decoding = esp-coredump (Espressif) from the PlatformIO penv, documented, not added.

Plan (minimal change surface, platform abstraction respected):
1. platform_esp32.h: platformResetRegisterDump() (today a no-op on ESP32, the reboot-log substitution point) returns one 'Core dump: <task> pc=0x.. bt=0x.. ..' line (<=119 chars, PCs capped to fit) when esp_core_dump_image_check()==ESP_OK and this dump has not been logged before (fingerprint of size+exc_pc+bt[0] in NVS via Preferences, so it is logged once and the dump itself stays in flash for a USB read + esp-coredump decode). Not gated on the current reset reason (an OTA after a crash is a software reset). For a panic/TWDT/INT_WDT reset without a readable dump it returns 'Core dump: none (<reason>)'. Panic reason + app ELF sha (first 8 hex) go to the telnet boot log.
2. helperStuff.ino isRebootLogDetailLine() accepts the 'Core dump:' prefix, so /api/v2/device/crashlog reports it (AC#2).
3. OTGW-Core.ino TWDT comment names every subscriber (AC#3).
4. Exploratory bench step BEFORE finalising: local build with a build-flag-gated loop hang (-DOTGW_TEST_TWDT_HANG, never committed, telnet key in a local patch) on .61 (OTA app via safeboot, settings saved first, hang only after the 30 s rollback window, no PIC touched). It shows what the summary and panic reason contain for a TWDT. If exc_task/backtrace do not point at the stalled loop, add step 5.
5. (only if 4 says so) IRAM-safe esp_task_wdt_isr_user_handler that copies the names of the tasks running on each core plus a loop-phase id into RTC_NOINIT; logged at the next boot. No calls back into IDF from the ISR (print_triggered_tasks may take the TWDT spinlock; unverifiable without IDF source).
No REST/MQTT contract change (crashlog JSON shape unchanged) -> no ADR. A core-dump download endpoint is out of scope (would need ADR-056 admin auth + ADR); USB read covers it.
Verification: host test on the real readLatestCrashLog/isRebootLogDetailLine slice (red on current code for a 'Core dump:' line, green on fix); bench forced TWDT -> crashlog available:true with task/backtrace, addr2line on the archived .elf resolves the hang site; build combo, evaluate --quick, ESP abstraction gate (no raw esp_core_dump_* outside platform_esp32.h).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench .61 (OTGW32-class test unit, alpha.512 -> exploratory local build alpha.535+8db0ede with -DOTGW_TEST_TWDT_HANG, never pushed/posted; settings saved first to the job dir, no PIC touched), 2026-10-29 21:3x local:
1. OTA app via safeboot OK (ota_flash.py --no-fs). 173 settings keys survive; the 4 that changed are other tasks' (satautogains removed by TASK-1357, 3 comfort keys by TASK-1441).
2. First boot after the OTA (software reset) already reported a core dump that had been sitting in flash from an earlier crash: reboot_log 21:35:27 'Software reset (3)' + 'Core dump: ipc1 pc=0x40383bf7 bt=...'. Confirms the dump is read independent of the reset reason (an OTA after a crash is a software reset). That dump belongs to an older build, so its addresses cannot be decoded with the current .elf: the line now carries elf=<8 hex> (app_elf_sha256 prefix; CONFIG_APP_RETRIEVE_LEN_ELF_SHA = 9) to pick the right .elf from build-archive.
3. Forced stall inside the async_tcp callback (telnet key, handled in onTelnetInput, i.e. in the async_tcp task) after >= 1 healthy minute: unit unreachable 6 s after the key, back 41 s after it; device/info lastreset 'Task watchdog'; /api/v2/device/crashlog available:true, summary '2026-10-29 21:37:29 - reboot cause: Task watchdog (6)', details 'Core dump: async_tcp pc=0x4205cb04 bt=0x4205cb04 0x4206eadd 0x42021175 0x40383841'. addr2line on the matching .elf: onTelnetInput <- SimpleTelnetCore::_handleCharInput (SimpleTelnetCore.h:734) <- AsyncSimpleTelnet::_onData (AsyncSimpleTelnet.h:422) <- AsyncClient::_recv (AsyncTCP.cpp:1068) <- _async_service_task (AsyncTCP.cpp:328): exactly the injected hang site. PASS for a hang in the async_tcp task.
4. reboot_log.txt after the change: 21 lines, 21 LF, 21 CR, 21 CRLF (before: 33 CR, 1 LF on the same unit); the line-break fix works on real data.
5. Not yet proven: the field case (loop task stalled, async_tcp idle). exc_task is the task running on the core that took the TWDT interrupt; whether that is loopTask when loopTask spins is untested. Next: second exploratory build with the hang in loop() (key Z) and the async hang kept on key Y.

Committed without the loop-hang bench proof (coordinator 75, 2026-10-30: .61 unplugged). Rebased onto origin/dev c00c5f025 (alpha.542). Fresh evidence on the rebased tree: tests/test_crashlog_coredump.py FIX 7/7 OK (exit 0); same test against c00c5f025's platform_esp32.h + helperStuff.ino (OTGW_PLATFORM_H/OTGW_HELPER_INO) FAILED failures=4 skipped=1 (exit 1); evaluate.py --quick 79 passed, 0 warn, 0 fail. AC#2 checked on bench run 1 (crashlog available:true, item 3) plus the host test. AC#3 checked: OTGW-Core.ino TWDT comment names the loop task and AsyncTCP async_tcp (espMqttClient internal task not used). AC#1 OPEN: proven for a hang in async_tcp only; the loop-task stall (key Z, exc_task when loopTask spins) waits for .61. No firmware build in this commit (batch cadence; coordinator builds). Hang test code (OTGW_TEST_TWDT_HANG) not committed; kept as a local patch.
<!-- SECTION:NOTES:END -->
