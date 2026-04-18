---
id: TASK-291
title: Fix ESP32-S3 flash_mode DIO/QIO mismatch between build and config
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 07:42'
updated_date: '2026-04-18 07:44'
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Not a bug. Closed as "working as designed".

Investigation:
ESP32 Arduino core 3.3.8 boards.txt line 1229 defines
  esp32s3.menu.FlashMode.qio.build.flash_mode=dio
  esp32s3.menu.FlashMode.qio.build.boot=qio

On ESP32-S3 the app image flash_mode is fixed to DIO by the core,
regardless of the user's selected FlashMode. The chip starts in DIO,
and the bootloader (which carries the real QIO config via build.boot)
switches the flash controller to QIO before jumping to the app. This
is the ESP32-S3 hardware contract, not an oversight in our build.py.

Our TARGETS[esp32s3].flash_mode=qio was correctly describing the
intended operating mode (the one in effect after boot); the image
header will always read DIO because the header encodes the startup
mode, not the operating mode.

Follow-up fix landed in the same commit: verify_image_header() now
normalises this documented override before comparing. An esp32s3
image with Flash mode: DIO matches a TARGETS[esp32s3].flash_mode=qio
config. Re-ran the smoke test against the current build artefact:
  otgw-esp32-test.bin: header OK ({'mode': 'qio', 'freq': '80m', 'size': '4mb'})
  verify result: True

No user-visible firmware change. PlatformIO path was investigated and
confirmed to follow the same core logic; no PIO action required.
<!-- SECTION:FINAL_SUMMARY:END -->
