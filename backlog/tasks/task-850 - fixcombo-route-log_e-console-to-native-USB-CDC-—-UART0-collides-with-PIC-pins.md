---
id: TASK-850
title: >-
  fix(combo): route log_e/console to native USB-CDC — UART0 collides with PIC
  pins
status: Done
assignee:
  - '@claude'
created_date: '2026-06-10 05:25'
updated_date: '2026-06-20 10:55'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Combo env has no ARDUINO_USB_CDC_ON_BOOT; default S3 console = UART0 on GPIO43/44 which ARE the combo PIC UART pins. Boot diagnostics (log_e detection line) go into the PIC instead of USB; field tester is blind. Add -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 to env:esp32-combo so log_e and Serial land on the native USB-Serial-JTAG port.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 env:esp32-combo build_flags carry ARDUINO_USB_MODE=1 and ARDUINO_USB_CDC_ON_BOOT=1
- [x] #2 esp32-combo build green; esp8266 and esp32 envs unchanged and green
- [ ] #3 Field: log_e boot-detect line visible on USB CDC console of LOLIN S3 Mini
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Root cause: env:esp32-combo had no USB-CDC flags; pioarduino default console = UART0 on GPIO43/44 = combo PIN_PIC_TX/PIN_PIC_RX. log_e boot diagnostics were transmitted into the PIC at 115200 instead of appearing on USB; UART0 console and OTGWSerial UART1 also contended for the same GPIO matrix pins. Fix: -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 in env:esp32-combo build_flags (combo-only; plain esp32 env has no pin collision on 43/44 and stays unchanged). Bumped alpha.170 -> alpha.171. Known residual: ROM bootloader still emits its 115200 boot log on GPIO43 into the PIC RX at every reset (eFuse-only to suppress; out of scope).

Audit 2026-06-13: blocker 1 resolved — the esp32-combo env with -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1 is now COMMITTED via TASK-864/ADR-127 (c2aef89c, platformio.ini env:esp32-combo); esp32-classic env already carried the flags. Remaining: AC#3 hardware validation (log_e visible on LOLIN S3 Mini native USB CDC console) — fold into the TASK-864 AC#6 field matrix; the combo boot log_e detect line is the natural test vehicle.

OBSOLETE (audit wp0vjoo5s): the USB-CDC console flags on env:esp32-combo shipped under TASK-864/ADR-127 (commit c2aef89c, Done, field-validated via /bootdetect.log). TASK-850 has no commit of its own; its sole field AC#3 is covered by TASK-864 AC#6. No separate work remains.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved-by-TASK-864. log_e/console-to-USB-CDC for esp32-combo is in-tree under c2aef89c (ADR-127) and field-validated under TASK-864 AC#6.
<!-- SECTION:FINAL_SUMMARY:END -->
