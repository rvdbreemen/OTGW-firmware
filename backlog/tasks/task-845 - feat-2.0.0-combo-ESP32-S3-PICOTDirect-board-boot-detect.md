---
id: TASK-845
title: 'feat-2.0.0: combo ESP32-S3 PIC+OTDirect board + boot detect'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 21:30'
updated_date: '2026-06-08 21:57'
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
- [ ] #2 BOARD_NODOSHOP_ESP32_COMBO in boards.h: HAS_PIC=1 AND HAS_DIRECT_OT=1, both pin sets, HAS_PIC_WATCHDOG flag
- [ ] #3 env:esp32-combo in platformio.ini lib_ignores neither OTGWSerial nor OpenTherm; type-stubs resolved; builds clean
- [ ] #4 settings.hw.iBoardMode persisted field (0=auto,1=pic,2=otdirect)
- [ ] #5 Boot reads persisted mode or runs PIC-probe-first detect, writes result back
- [ ] #6 initOTDirect no longer clobbers PIC eMode unconditionally
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
<!-- SECTION:NOTES:END -->
