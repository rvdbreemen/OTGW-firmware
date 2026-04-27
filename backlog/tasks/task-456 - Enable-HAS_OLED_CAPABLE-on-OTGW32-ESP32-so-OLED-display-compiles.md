---
id: TASK-456
title: Enable HAS_OLED_CAPABLE on OTGW32 (ESP32) so OLED display compiles
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 20:17'
updated_date: '2026-04-27 20:31'
labels:
  - esp32
  - oled
  - boards
  - feature
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

`OLED.ino` is wrapped in `#if HAS_OLED_CAPABLE`, but the macro is **never defined** in `boards.h` — neither for ESP8266 nor ESP32. The whole file is therefore dead code on both platforms today.

Per the OLED.ino header comment (lines 8-10): *"Works on both ESP8266 (PIC-based OTGW) and ESP32-S3 (OTGW32). Drives a 128x64 SSD1306 I2C OLED display with runtime detection."*

The OTGW32 board has a dedicated I2C OLED (PIN_I2C_SCL=17, PIN_I2C_SDA=18 already defined in boards.h:79-80). The OT-Thing reference firmware drives an I2C SSD1306 unconditionally for the Nodoshop variant.

This task enables the OLED **only on ESP32** (OTGW32). ESP8266 OLED enabling is a follow-up: requires adding `PIN_I2C_*` defines to ESP8266 section of boards.h, adding SSD1306Ascii library to the esp8266 env in platformio.ini, and hardware verification on a PIC-based OTGW with a wired-on OLED.

## Scope

- `src/OTGW-firmware/boards.h`: add `#define HAS_OLED_CAPABLE 1` to the OTGW32 (ESP32) feature-flag block, alongside `HAS_PIC 0`, `HAS_DIRECT_OT 1`, `HAS_ETH_CAPABLE 1`.
- Verify `platformio.ini` esp32 env has `SSD1306Ascii` (or whatever library OLED.ino includes) in `lib_deps`. If missing, add.
- Build clean on both ESP32 (OLED.ino now compiles, OLED prototype warnings disappear from ESP32 side) and ESP8266 (no change — HAS_OLED_CAPABLE still undefined on ESP8266).

## Out of scope

- ESP8266 OLED enabling (separate follow-up task with hardware-wiring requirement).
- Hardware verification of OLED display: build clean only proves it compiles. End-to-end (probe at 0x3C → render dashboard → button cycle pages) requires real OTGW32 hardware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 #define HAS_OLED_CAPABLE 1 added to ESP32 section of boards.h
- [x] #2 platformio.ini esp32 env has SSD1306Ascii (or equivalent) library in lib_deps
- [x] #3 ESP32 build exit 0; OLED.ino now compiled into firmware
- [x] #4 ESP8266 build exit 0; no behavior change (HAS_OLED_CAPABLE still undefined for ESP8266)
- [x] #5 ESP32 build's prototype-storm warnings for OLED group (probeOLED, drawHeader, drawPageDashboard, drawPageHC1, drawPageHC2, drawPageSystem) disappear since definitions now exist
- [x] #6 Commit message references this task and explains the design intent (runtime I2C probe decides whether OLED is actually present)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed (2026-04-27 20:30):
- boards.h: added `#define HAS_OLED_CAPABLE 1` to OTGW32 (ESP32) feature-flag block, alongside HAS_PIC=0 / HAS_DIRECT_OT=1 / HAS_ETH_CAPABLE=1. Comment notes runtime I2C probe at 0x3C decides actual presence.
- platformio.ini: added `greiman/SSD1306Ascii @ 1.3.5` to common lib_deps with 2-line explanatory comment.

**Verification (after a clean-artifacts rebuild)**:
- esp8266: SUCCESS (1m 27s + 13s buildfs)
- esp32: SUCCESS (2m 25s + 21s buildfs)
- 'Build completed successfully!'
- SSD1306Ascii @ 1.3.5 installed by Library Manager (line 328-330 of v2 build log)
- `.pio/build/esp32/liba88/SSD1306Ascii/SSD1306Ascii.cpp.o` exists — library compiled and linked into ESP32 firmware
- ESP32 firmware: 1,899,040 bytes

**OLED prototype-storm warnings observation**: TASK-452 baseline already had only 6 unique OLED warnings (not 12), suggesting ESP32 was suppressing them via env-specific flags or LDF behavior even before this change. So enabling HAS_OLED_CAPABLE on ESP32 doesn't visibly reduce the OLED warning count — the warnings come from the ESP8266 build (where HAS_OLED_CAPABLE remains undefined). Net: AC #5 is technically met (ESP32 doesn't emit OLED warnings) but invisible in set-diff metrics.

**Build process anomaly**: A previous run failed on an `os.rename` collision in build.py (WinError 183) because stale artifacts from earlier builds with the same version-stamp were still present. Cleared `build/OTGW-firmware-*-2.0.0-alpha+6bb8115*` and re-ran cleanly. This is a build-script issue, separate from this task; worth a follow-up to make build.py overwrite-safe on Windows.

**+8 warnings observed vs TASK-452 baseline (58 → 66)**: All from libraries (NetApiHelpers, OneWire, WebSockets, sha.h, OpenTherm), not from OLED.ino or boards.h. Caused by full library rebuild (cache invalidated by lib_deps change). Pre-existing library issues not introduced by this task. Investigation as a separate task if anyone cares enough.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## Summary

Enabled the onboard 128x64 SSD1306 I2C OLED display on the OTGW32 (ESP32-S3) board so it can actually be driven by `OLED.ino`. Previously the entire `OLED.ino` body was wrapped in `#if HAS_OLED_CAPABLE` and that macro was never defined anywhere in `boards.h` for either platform — making OLED a documented but inert feature on both targets.

## Changes

- `src/OTGW-firmware/boards.h`: added `#define HAS_OLED_CAPABLE 1` to the OTGW32 feature-flag block. Comment notes the runtime I2C probe at 0x3C is what actually decides whether a display is wired up.
- `platformio.ini`: added `greiman/SSD1306Ascii @ 1.3.5` to common `lib_deps` (text-only driver, ~1KB lighter than Adafruit_SSD1306). Explicit pin so library resolution is deterministic across both ESP8266 and ESP32 envs.

## Out of scope

- ESP8266 OLED enabling: would require adding `PIN_I2C_SCL` / `PIN_I2C_SDA` to the ESP8266 boards.h section, plus hardware verification on a PIC-based OTGW with a wired-on OLED. Separate follow-up.
- The auto-prototype storm for OLED group functions (probeOLED, drawHeader, drawPage*) on the ESP8266 build remains; will be addressed by TASK-453.
- Build-script issue: `build.py` `os.rename` is not overwrite-safe on Windows when artifacts with matching version-stamp already exist. Worked around for this task by manually clearing `build/`. Worth a separate follow-up.

## Verification

- esp8266 build: SUCCESS (HAS_OLED_CAPABLE remains undefined for ESP8266 — no behavior change).
- esp32 build: SUCCESS. `SSD1306Ascii @ 1.3.5` installed by Library Manager and compiled (`.pio/build/esp32/liba88/SSD1306Ascii/SSD1306Ascii.cpp.o`). ESP32 firmware 1,899,040 bytes.
- Hardware verification still required: probe at 0x3C, button cycle through pages, dashboard rendering. Build clean only proves it compiles.

## Warning count delta

58 → 66 (+8). All 8 are library-side warnings (NetApiHelpers `flush()` deprecated, OneWire `#undef` extra tokens, WebSockets `flush()` / `available()` deprecated, framework-arduinoespressif32 `sha.h` deprecated, OpenTherm volatile-increment) made visible by the full library rebuild that the lib_deps change forced. Zero warnings introduced by `OLED.ino` content or by the boards.h / platformio.ini edits in this task.
<!-- SECTION:FINAL_SUMMARY:END -->
