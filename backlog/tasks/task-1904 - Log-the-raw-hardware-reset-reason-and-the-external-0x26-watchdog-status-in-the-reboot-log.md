---
id: TASK-1904
title: >-
  Log the raw hardware reset reason and the external 0x26 watchdog status in the
  reboot log
status: To Do
assignee: []
created_date: '2026-11-13T16:52:00Z'
labels:
  - bug
  - stability
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1865/1899 trace (gruijter beta.218, 13 Nov): two resets were reported as 'Other watchdog' (ESP_RST_WDT, platform_esp32.h:216), which covers RTC, super and CPU-level watchdogs, and left no core dump, so the actual watchdog cannot be told apart. On the Classic the 'Reset by External WD' text from the 0x26 probe goes into a local buffer at OTGW-firmware.ino:545 that is discarded, and the boot telnet line comes before a capture reconnects. Persist in reboot_log.txt / device info: the raw hardware reset cause (rtc_get_reset_reason / esp_rom reset reason per core, the 'rst:0x..' code) and whether the 0x26 watchdog reported an external reset, so field resets can be classified without a USB console.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 reboot_log.txt and /api/v2/device/info carry the raw per-core hardware reset reason code next to the mapped text
- [ ] #2 On the Classic the 0x26 external-reset indication is persisted to the reboot log
- [ ] #3 Bench: a forced task-watchdog reset and a forced RTC-watchdog reset produce different, correct entries
<!-- AC:END -->
