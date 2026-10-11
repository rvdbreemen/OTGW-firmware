---
id: TASK-1218
title: >-
  Fix: the PIC-flash progress LEDs never toggle on ESP32-S3 builds (digitalWrite
  on LEDC pins is refused)
status: To Do
assignee: []
created_date: '2026-10-11 23:22'
labels:
  - bug
  - esp32
  - pic
  - led
dependencies: []
priority: low
ordinal: 338000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on the Classic + LOLIN S3 Mini bench, 2026-10-12, while verifying TASK-1129. During a PIC flash the firmware drives the LEDs with digitalWrite (fwupgradestart 'LED1 activated', OTGWSerial::SetLED via activeLed2()), but the LED pins are attached to LEDC (OTGW-firmware.ino ledcAttachChannel). arduino-esp32 3.x __digitalWrite() refuses a pin that is not a GPIO bus and only logs '[E][esp32-hal-gpio.c:181] __digitalWrite(): IO 16 is not set as GPIO' (LED1) and 'IO 4 ...' (LED2). So the flash progress LED never changes. On builds since alpha.364 the log line is muted, so the failure is silent. Before alpha.364 these lines went into the PIC over UART0 and broke the bootloader handshake (TASK-1129).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 LED writes during a PIC flash use the LEDC API (ledcWrite) or detach and reattach the pin, through a platform shim, so the LED visibly toggles
- [ ] #2 Bench: during a PIC flash on the Classic + S3 Mini the LED toggles (photo or observation noted), and no 'is not set as GPIO' line appears (checked on a build without the console mute or via a debug counter)
- [ ] #3 Build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->
