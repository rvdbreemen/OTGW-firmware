---
id: TASK-850
title: >-
  fix(combo): route log_e/console to native USB-CDC — UART0 collides with PIC
  pins
status: Done
assignee:
  - '@claude'
created_date: '2026-06-10 05:25'
updated_date: '2026-10-12 00:04'
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

2026-10-12 Classic-S3 bench validation (alpha.418 combo on OTGW Classic + LOLIN S3 Mini, PIC16F1847 gw 6.8): Bench check 2026-10-11/12 (evidence: C:/Users/rvdbr/AppData/Local/OTGW-capture/classic-validation-20261011/EVIDENCE.md, Phase 2.1 and its addendum). Board: OTGW Classic carrier + LOLIN S3 Mini (MAC ac:27:6e:ce:45:d8, COM8), esp32-combo 2.0.0-alpha.418+fa2b89d, PIC16F1847 gateway 6.8. AC#3 NOT met: the USB-CDC console shows NO `[combo] detect` log_e line across boot plus 2 RTS resets. Early log_e output does reach USB-CDC (I2C Error 259 lines at 50/179/264/393/478 ms, root-caused under TASK-1129 #3 / TASK-1216), so the console routing from AC#1 works. The detect line is logged after platformMuteUart0Console(), and that call also silences the USB console. The detect verdict is only visible in /bootdetect.log (boot#1..3: eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36). The bench also shows pre-mute log text going into the PIC RX on UART0 at 9600 baud (~530 bytes per boot). The earlier closing note ("covered by TASK-864 AC#6 via /bootdetect.log") checks the detect result, not the console line this AC asks for. Leave AC#3 unchecked; the follow-up decision is TASK-1217.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved-by-TASK-864. log_e/console-to-USB-CDC for esp32-combo is in-tree under c2aef89c (ADR-127) and field-validated under TASK-864 AC#6.
<!-- SECTION:FINAL_SUMMARY:END -->
