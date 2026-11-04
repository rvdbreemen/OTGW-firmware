---
id: TASK-845
title: 'feat-2.0.0: combo ESP32-S3 PIC+OTDirect board + boot detect'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-08 21:30'
updated_date: '2026-10-12 00:04'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Single ESP32-S3 binary supporting both PIC (OTGW Classic) and OTDirect (OTGW32). First-boot PIC-ETX-probe-first detection, persisted to LittleFS via settings.hw.iBoardMode, web-UI manual override. Phase 1 of 3 (see plan). Supersedes ADR-113.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New ADR supersedes ADR-113, passes 4 adr-kit gates
- [x] #2 BOARD_NODOSHOP_ESP32_COMBO in boards.h: HAS_PIC=1 AND HAS_DIRECT_OT=1, both pin sets, HAS_PIC_WATCHDOG flag
- [x] #3 env:esp32-combo in platformio.ini lib_ignores neither OTGWSerial nor OpenTherm; type-stubs resolved; builds clean
- [x] #4 settings.hw.iBoardMode persisted field (0=auto,1=pic,2=otdirect)
- [x] #5 Boot reads persisted mode or runs PIC-probe-first detect, writes result back
- [x] #6 initOTDirect no longer clobbers PIC eMode unconditionally
- [ ] #7 Web-UI/REST manual override sets iBoardMode + reboots
- [ ] #8 Field-validated on both hardwares: correct hardwaremode reported and persisted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 1 impl (pin-independent + combo board):
- ADR-125 drafted (Proposed), amends ADR-113 (runtime hardware_type + lifts 'never a PIC on ESP32' for combo class).
- boards.h: BOARD_NODOSHOP_ESP32_COMBO = OTGW32 base + PIC delta (HAS_PIC=1, HAS_DIRECT_OT=1); new HAS_PIC_WATCHDOG flag (ESP8266=1, ESP32=0, combo=0). PIC pins CANDIDATES (CONFIRM-BEFORE-FLASH): RST=5, RX=44, TX=43, PIC-mode I2C SDA=35/SCL=36 (LOLIN S3 mini labels; no overlap w/ OTGW32 map).
- Watchdog gate split: 0x26 ext WD now #if HAS_PIC_WATCHDOG (combo/OTGW32 use ESP32 TWDT); hexheaders[] stays #if HAS_PIC.
- settings.iBoardMode (top-level, KISS — deviates from plan's settings.hw section): 0=auto/1=pic/2=otdirect; load/save/display wired in settingStuff.ino. Override reachable via existing settings API today.
- setup(): detection moved AFTER LittleFS+readSettings; combo PIC-probe-first orchestration + persist; fixed boards unchanged.
- initOTDirect():842 no longer clobbers HW_MODE_PIC (guarded by !isPICEnabled()).
- activeI2cSda/Scl() helper: combo PIC mode uses D1-mini I2C pins; Wire.begin sites updated (setup + ESP32 initWatchDog).
- W5500 initEthernet() skipped in PIC mode.
- platformio.ini: env:esp32-combo (extends esp32, drops -DOTGWFirmware=int, lib_ignore cleared).
Pending: build.py combo target; web-UI override control; Phase 2 (runtime #if->if conversions) + Phase 3 (runtime HA identity) gated on field validation.

Phase 1 committed 6dcbd395 + pushed. Combo build green (Flash 98.1%, RAM 35.2%), evaluator 0 failures (abstraction back to baseline 4 via new HAS_RUNTIME_HW_DETECT flag). Remaining: AC7 dedicated web-UI override control (mechanism already reachable via settings API), AC8 field validation on real hardware + PIC pin confirmation. Phase 2=TASK-846, Phase 3=TASK-847.

Field fix (alpha.169): LOLIN S3 Mini in OTGW Classic socket booted but parked in WiFi portal with starved webserver + task_wdt spam. Fixes: PIN_PIC_RST 5->12 (D5 hole = S3 SCK=GPIO12, corroborated by OT-Thing W5500 SCK=12); feedWatchDog guarded behind TWDT-ready flag (kills 'task not found' spam); OTGWSerial.end() on no-PIC before initOTDirect (kills floating-UART RX-storm webserver-starvation suspect); boot detection result logged via log_e to USB/IDF console for ground truth. New doc docs/hardware/combo-esp32-s3-pinout.md (D1-mini<->S3-Mini footprint cross-reference). Combo build green.

Pinout docs completed: all D1-mini footprint holes confirmed from official LOLIN S3 Mini pin diagram (A0=2, D0=4, D3=18, D4=16, D8=10). Boot-critical pins unchanged. Auxiliary-hole overlaps with OTGW32 map documented (button 18=I2C SDA, LED1 16=W5500 RST, LED2 4=1-Wire); Classic LEDs/button not driven in PIC mode today.

Outcome: combo experiment retired after field testing (alpha.166-172). Runtime detection never converged in the field (portal starvation, pin-map iterations, unfed 0x26 watchdog). Superseded by ADR-126 / TASK-854: fixed esp32-classic build replaces the combo.

2026-10-12 Classic-S3 bench validation (alpha.418 combo on OTGW Classic + LOLIN S3 Mini, PIC16F1847 gw 6.8): Bench validation 2026-10-11/12, Classic half only. Evidence: C:/Users/rvdbr/AppData/Local/OTGW-capture/classic-validation-20261011/EVIDENCE.md. Hardware: OTGW Classic carrier + LOLIN S3 Mini (MAC ac:27:6e:ce:45:d8, 192.168.88.63), PIC16F1847 gateway 6.8. Firmware: esp32-combo 2.0.0-alpha.418+fa2b89d, app + fs flashed. AC#8 (Classic half): device/info reports hardware_type otgw-classic, hardwaremode PIC, otcommandinterface PIC and otdirectavailable false. /bootdetect.log shows boot#1-3 all `eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36`; those boots were the fs flash plus 2 RTS resets, not a power cycle. Auto mode (0) persists boardmode=1 after a positive PIC detect; this is by design, see OTGW-firmware.ino:405-412. AC#7 (REST half only): a REST POST of boardmode, followed by a separate POST /ReBoot, gave: mode 1 -> boot 5, PIC, setting 1. Mode 2 -> boot 6, corrected back to 1 by TASK-1031 recovery, PIC found. Mode 0 -> boot 7, PIC found, setting persisted as 1. Mode 3 was skipped (wrong pin map for this board). Still open: (a) the Web-UI override control, and proof that the override reboots by itself rather than through a separate /ReBoot; (b) the OTGW32/OT-Direct half of AC#8, including mode 2 actually taking effect. Doc drift: the comment at OTGW-firmware.ino:341-346 ('auto NEVER persists its verdict', TASK-947) is stale. Task bookkeeping: TASK-845 is marked Done with the outcome 'combo retired (ADR-126/TASK-854)', but the combo build exists again and was used here. The status and the outcome text need a maintainer review.
<!-- SECTION:NOTES:END -->
