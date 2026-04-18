---
id: TASK-291
title: Fix ESP32-S3 flash_mode DIO/QIO mismatch between build and config
status: To Do
assignee: []
created_date: '2026-04-18 07:42'
labels:
  - esp32
  - build
  - flash
dependencies:
  - TASK-290
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The TASK-290 verify-image helper caught a real drift on the current ESP32-S3 build: TARGETS[esp32s3].flash_mode=qio in build.py (and platformio.ini has board_build.flash_mode=qio), but the esptool image-info readout shows Flash mode: DIO. arduino-cli apparently uses DIO as default for esp32s3-devkitc-1 via the core boards.txt FlashMode menu, overriding our intended qio config. QIO doubles SPI bandwidth to flash, so the mismatch costs startup/code-read performance and forces the chip to run below its capability. PlatformIO path needs verification too (board_build.flash_mode may need to be confirmed end-to-end).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ESP32-S3 arduino-cli build emits an image with Flash mode: QIO per esptool image-info
- [ ] #2 PlatformIO ESP32-S3 build emits an image with Flash mode: QIO per esptool image-info
- [ ] #3 build.py --verify-image passes without flash_mode mismatch on both backends
- [ ] #4 Investigation documents which knob fixes it: (a) --fqbn FlashMode=qio suffix, (b) --build-property build.flash_mode=qio, or (c) a custom board variant JSON
<!-- AC:END -->
