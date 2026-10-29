---
id: TASK-1479
title: >-
  Task-watchdog reboots cannot be diagnosed: the crash log never reports an
  ESP32 panic and the core dump is never read
status: To Do
assignee: []
created_date: '2026-10-29T14:37:00Z'
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
- [ ] #2 The crash-log endpoint reports an ESP32 panic/TWDT reboot instead of available:false
- [ ] #3 The OTGW-Core.ino TWDT comment names every task subscribed to the TWDT
<!-- AC:END -->
