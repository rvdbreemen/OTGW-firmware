---
id: TASK-864
title: >-
  feat(combo): revive single ESP32-S3 binary with runtime PIC/OTDirect boot
  detection (ADR-127, supersedes ADR-126)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-12 22:14'
updated_date: '2026-06-12 23:37'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Revival of ADR-125 combo approach now that root cause is fixed: OTGWSerial TU could not see boards.h and probed on default GPIO 16/17 instead of 43/44 (fixed 77f084b0), plus field-verified Classic-on-S3 pin map. New esp32-combo build target (4th, transitional) links both OT engines and selects mode at boot via PIC-probe-first detection, persisted in settings.iBoardMode (ADR-125 persistence behaviour). Runtime accessors for I2C/LED/button pins, HAS_PIC_WATCHDOG=1 with runtime 0x26 feed gating, runtime hardwareTypeName(), REST + web-UI boardmode override. Plan: C:/Users/rvdbr/.claude/plans/so-now-that-we-memoized-flame.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Phase 1: boards.h BOARD_NODOSHOP_ESP32_COMBO + platformio.ini env:esp32-combo + build.py/evaluate.py target; pio run -e esp32-combo links with flash <100% (record %)
- [x] #2 Phase 2: runtime machinery revived (settings.iBoardMode, detection block post-startWiFi, activeI2c/Led/Button accessors, runtime hardwareTypeName, watchdog dual-path dispatch, handlePICSerial+60s probe gated on !isOTDirectEnabled, /bootdetect.log, settingStuff boardmode sites)
- [x] #3 Web-UI boardmode dropdown (auto/pic/otdirect) wired to settings API
- [x] #4 ADR-127 written and Accepted (supersedes ADR-126, root-cause evidence table, TASK-847 deferral documented); PINOUT.md updated
- [x] #5 python build.py all 4 targets green; evaluate.py --quick green; ESP-abstraction baseline not raised
- [ ] #6 Field validation: combo on Classic PCB detects PIC (mode 1, 0x26 fed, OLED 36/35, LEDs 16/4); combo on OTGW32 selects OTDirect (mode 2, Ethernet, OLED 17/18); portal responsive on fresh flash; override via REST+UI works
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Approved master plan at C:/Users/rvdbr/.claude/plans/so-now-that-we-memoized-flame.md. Phases: 1) boards.h combo section + build plumbing, flash-budget gate; 2) runtime detection machinery (revive from ec55a1fb with pin-fix adaptations); 3) ADR-127 + docs; 4) build/evaluator/field verification. Maintainer decisions: 4th transitional target, ADR-125 persistence, REST+UI override, TASK-847 OtCore deferral.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 1+2+UI implemented. esp32-combo links: Flash 98.4% (1935391/1966080, ~30KB headroom), RAM 35.3%. Key deltas vs ADR-125 era: PIN_CLASSIC_* prefixed pin set + comboClassicPinsActive() resolution (LED/button/I2C all runtime now, Classic default pre-detection); applyResolvedComboPins() re-mux after detection (Wire end+begin, ledc re-attach, initOLED re-run); watchdog three-way dispatch (combo = TWDT + 0x26 gated on isPICEnabled); handlePICSerial drain + 60s PR=A retry gated on !isOTDirectEnabled (banner recovery TASK-861 preserved on PIC-mode/degraded boots); banner recovery persists iBoardMode=1 on combo; telnet 'p' blocked in OTDirect mode (PIC_RST=GPIO12=SPI SCK). boardmode exposed via REST (combo-only) + UI dropdown in System group with reboot-confirm. Regression builds of fixed targets + ADR-127 authoring in flight.

Committed c2aef89c (alpha.176), pushed to origin/feature-dev-2.0.0. ADR-127 Accepted (quality 0.86/A); ADR verification pass caught one omission (Ethernet gate if (!isPICEnabled()) initEthernet() missing) — restored, combo rebuilt green 98.4%. Evaluator: 0 failed / 97.1%. Remaining: AC6 field validation (Classic PCB + OTGW32 hardware matrix). Full build.py packaging run in background.

Field validation Classic PCB (S3 Mini, combo build, 2026-06-13): PASS on first matrix leg. /bootdetect.log: boot#1 fresh-flash detect at t=130948ms (portal-first order confirmed working, detection after WiFi config), eMode=1 pic=1 mode=1 persisted; boot#2 fast-path detect at t=4477ms via persisted mode=1. Pins verified live: RST=12 RX=44 TX=43 I2C(classic)=35/36. PIC fully functional: PR=G/I/L/T/D/M round-trips, gateway mode ON, settings readout cycle, gateway.hex 6.6 recognized. Heap healthy 92-133KB, 0 drops, no resets over 3.5min window. 0x26 watchdog feed in PIC mode confirmed implicitly: >=3.5min armed without external-WD reset. MQTT first-seen log spam explained: no broker connected, ADR-104 slots never confirm — by design, not a bug (optional cosmetic: mute gate-log while MQTT disconnected). REMAINING matrix: OLED splash on 36/35 (if OLED fitted), LEDs 16/4 + button 18, PIC fw upgrade via ModUpdateServer, REST hardware_type=otgw-classic check, combo on OTGW32 leg, boardmode override round-trip.
<!-- SECTION:NOTES:END -->
