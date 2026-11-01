---
id: TASK-1438
title: >-
  Bench Classic+S3: three reboots 'reboot cause: Unknown (b)' within 90 s on
  alpha.519 during a replay run
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T22:18:00Z'
updated_date: '2026-10-31T23:00:00Z'
labels:
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 599000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2026-10-28 21:20:27, 21:21:31, 21:21:56 on the Classic+S3 (.63), alpha.519+49135fe, SAT on, OT replay running (TASK-1420 bench run, S4 first attempt). /reboot_log.txt: 'reboot cause: Unknown (b)', no crashlog. No USB, no PIC actions. Cause code 0xb not mapped yet. Logs: %TEMP%\otgw-bench-1420\A_519\S4_try2_reboot.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reset reason 0xb mapped against the ESP-IDF reset reasons and the firmware's reboot log code; cause explained or reproduced
- [x] #2 If a firmware cause: fix with red/green evidence; else documented as external
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Mapped: the reboot log code is esp_reset_reason() (platformResetCode, platform_esp32.h:523); 0xb = 11 = ESP_RST_USB per ESP-IDF esp_system.h:25-36 (framework-arduinoespressif32-libs, enum order). On the ESP32-S3 that is a reset by the USB-Serial-JTAG peripheral, triggered when the USB host toggles the serial lines (a tool opening or probing the COM port); the .63 unit was on USB to the bench PC. Not a firmware cause: documented as external. platformResetReason() now names reasons 11-15 (USB, JTAG, eFuse, power glitch, CPU lock-up), so the log reads 'USB reset (host) (b)' instead of 'Unknown (b)'.
Evidence: tests/test_reset_reason_names.py new 3/3 (incl. reading the installed esp_system.h enum: ESP_RST_USB at index 11 and every member has a case); old platform_esp32.h fails for 11-15. evaluate 0 FAIL.
Not verified: which host process opened COM8 at 21:20-21:21 on 2026-10-28 (no process log); a bench repro of a USB reset was not run (.63 not connected).
<!-- SECTION:FINAL_SUMMARY:END -->
