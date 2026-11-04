---
id: TASK-1218
title: >-
  Fix: the PIC-flash progress LEDs never toggle on ESP32-S3 builds (digitalWrite
  on LEDC pins is refused)
status: Done
assignee: []
created_date: 2026-10-11 23:22
updated_date: '2026-11-04T07:08:00Z'
labels:
  - bug
  - esp32
  - pic
  - led
dependencies: []
priority: medium
ordinal: 338000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on the Classic + LOLIN S3 Mini bench, 2026-10-12, while verifying TASK-1129. During a PIC flash the firmware drives the LEDs with digitalWrite (fwupgradestart 'LED1 activated', OTGWSerial::SetLED via activeLed2()), but the LED pins are attached to LEDC (OTGW-firmware.ino ledcAttachChannel). arduino-esp32 3.x __digitalWrite() refuses a pin that is not a GPIO bus and only logs '[E][esp32-hal-gpio.c:181] __digitalWrite(): IO 16 is not set as GPIO' (LED1) and 'IO 4 ...' (LED2). So the flash progress LED never changes. On builds since alpha.364 the log line is muted, so the failure is silent. Before alpha.364 these lines went into the PIC over UART0 and broke the bootloader handshake (TASK-1129).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 LED writes during a PIC flash use the LEDC API (ledcWrite) or detach and reattach the pin, through a platform shim, so the LED visibly toggles
- [ ] #2 Bench: during a PIC flash on the Classic + S3 Mini the LED toggles (photo or observation noted), and no 'is not set as GPIO' line appears (checked on a build without the console mute or via a debug counter)
- [x] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: OTGW-Core.ino:6395 and OTGWSerial.cpp:1022 digitalWrite on LEDC-attached LED pins (OTGW-firmware.ino:664-669). TASK-1552 did not touch it. Agent-doable, AC2 bench on .63.

2026-11-04 Robert: AC2 (LED toggles during a real PIC flash) waived; prove the fix without flashing the PIC (drive the same LED code path).
2026-11-04 (sonnet agent, lead reran in the integration tree): picProgressLedWrite() -> setLed() (ledcWrite on LEDC boards) for LED1 at flash start; OTGWSerial gets registerLedCallback() (same pattern as registerFirmwareCallback) so SetLED() uses the app's LEDC-aware write; without a callback the old digitalWrite path is unchanged. test/host/test_pic_led_ledc.py --old-rev HEAD: OLD L1 refused=1, L2 refused=3 (FAIL); FIX L1/L2/P1 pass; RESULT PASS. evaluate 0 failed. AC2 waived by Robert (no PIC flash). Tag beta.64.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PIC-flash progress LEDs use the LEDC-aware LED write on the ESP32-S3 (OTGWSerial LED callback -> setLed()). Evidence: test/host/test_pic_led_ledc.py old L1/L2 refused (FAIL), fix pass; beta.64 runs on both bench units without regression (no crashlog). AC2 (LED toggles during a real PIC flash) waived by Robert 2026-11-04: no PIC flash.
<!-- SECTION:FINAL_SUMMARY:END -->
