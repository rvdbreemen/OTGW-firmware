---
id: TASK-1221
title: >-
  Classic + LOLIN S3 Mini: drive the on-board WS2812B RGB pixel (GPIO47) as a
  status indicator
status: Done
assignee:
  - '@claude'
created_date: '2026-10-12 16:50'
updated_date: '2026-10-12 17:44'
labels:
  - ui
  - led
  - classic
dependencies: []
priority: low
ordinal: 341000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The LOLIN S3 Mini carries a WS2812B RGB pixel on GPIO47 (field report from tjfs in Discord, 2026-10-12). Maintainer decision: drive the pixel ONLY when the board runs as an OTGW Classic with a LOLIN ESP32-S3 (PIC mode, combo detection); never on the OTGW32. Conflict to resolve first: boards.h maps GPIO47 to PIN_BYPASS_RELAY on the OTGW32 map, and OTDirect.ino:810-811/2510-2553 drives it, so a LOLIN that falls back to OT-Direct (no PIC) would toggle the pixel data line as a relay. Colour scheme still to decide (e.g. AP active / WiFi connected / PIC online). Needs a HAS_* flag in boards.h and a platform shim (no raw board macros in application code); RMT-based driver from the Arduino core (rgbLedWrite / neopixelWrite) avoids a new library.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Pixel shows the agreed status colours on an OTGW Classic + LOLIN S3 Mini in PIC mode (bench photo or screenshot)
- [x] #2 Colour scheme agreed with the maintainer before implementation
- [x] #3 build.bat --target esp32-combo and esp32-classic green, evaluate.py --quick no new FAIL
- [x] #4 S3 Mini Pro: pixel on GPIO8 with GPIO7 power shows the same colours (bench photo); colour order GRB confirmed or corrected
- [x] #5 OTGW32 never drives the pixel: /api/v2/device/info on the bench OTGW32 (psram_found 0) reports status_pixel_gpio 0, device boots and runs as before
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Colour scheme (maintainer, 2026-10-12): WiFi not connected = flash RED; WiFi captive portal (config AP) = flash PURPLE; WiFi connected = flash GREEN.

Correction to the description: HAS_BYPASS_RELAY is defined nowhere in boards.h, so every #if HAS_BYPASS_RELAY block in OTDirect.ino compiles out; GPIO47 has no other driver and there is no conflict. Maintainer decisions 2026-10-12: LOLIN identified by psramFound() (LOLIN S3 Mini/Pro = ESP32-S3FH4R2 with in-package PSRAM; bench OTGW32 reports psram_found 0, chip_model_est ESP32-S3FN4), because startWiFi() and its blocking portal run before PIC detection; AP fallback = PURPLE; S3 Mini Pro included (core variant lolin_s3_mini_pro: RGB_BUILTIN GPIO8, RGB_POWER GPIO7, GPIO47 = BUTTON_OK; plain lolin_s3_mini: GPIO47, LED_COLOR_ORDER_RGB). Implementation: boards.h HAS_STATUS_PIXEL + pins (combo and esp32-classic), platformPixelWrite() shim over rgbLedWriteOrdered, OTGW-firmware.ino startStatusPixel() before startWiFi(), Ticker 500 ms half period, state derived each tick from WiFi.getMode()&AP / WiFi.status().

alpha.420: build.bat esp32-combo and esp32-classic both 'Successfully created ESP32S3 image' / SUCCESS (first classic attempt failed on state.hw.bClassicPro, which only exists under HAS_RUNTIME_HW_DETECT; fixed with a local pro flag, false on the fixed Classic target). evaluate.py --quick: 0 failed, 0 warnings. Bench OTGW32, app-only flash of the combo image: device/info psram_found 0, status_pixel_gpio 0, OT-Direct, MQTT connected, 0/182 settings changed (EVIDENCE.md folder, 12_otgw32_pixel_1221.txt). Open: AC1 (Classic + S3 Mini, colours) and AC4 (S3 Mini Pro, GRB check) need those boards on the bench.

Bench Classic + LOLIN S3 Mini (COM8, AC:27:6E:CE:45:D8, .88.63), app-only flash combo alpha.420: device/info psram_found 1, chip_model_est ESP32-S3FH4R2, status_pixel_gpio 47, PIC 6.8 gateway, MQTT connected (14_classic_pixel_1221.txt). Maintainer observed: GREEN pixel flashing once per second with WiFi connected; confirms GPIO47 and the RGB colour order (GRB would show red). Red (boot before WiFi) and purple (AP) still to observe.

Maintainer observed: RED pixel flashing during reboot before WiFi connects, then GREEN. Purple (AP) not yet observed.

Maintainer observed PURPLE with an access point up. All three colours confirmed on Classic + LOLIN S3 Mini (green connected, red during boot before WiFi, purple with AP). Device back online without reboot (bootcount 18), PIC 6.8, MQTT connected, 0 settings changed (13 vs 15 settings JSON). Remaining: AC4 (S3 Mini Pro on GPIO8/GPIO7, GRB order) needs a Pro on the bench; task stays In Progress for that AC only.

AC4 checked on maintainer instruction (2026-10-12: 'ga nu maar ac#4 ook afvinken, we gaan even geen PRO testen'). NOT bench-tested: the S3 Mini Pro path (GPIO8 data, GPIO7 power, GRB order) rests on the core variant lolin_s3_mini_pro/pins_arduino.h only. If a Pro shows swapped colours, flip statusPixelRgbOrder for the Pro branch in startStatusPixel().
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
LOLIN on-board WS2812B pixel shows the WiFi state on an OTGW Classic, flashing at 1 Hz: red = not connected, purple = access point up (config portal or AP fallback), green = connected. A Ticker drives it (the WiFiManager portal blocks setup()); the colour is re-derived each tick from WiFi.getMode()/WiFi.status(). The LOLIN is recognised by psramFound() because the portal runs before PIC detection. S3 Mini: GPIO47, RGB order; S3 Mini Pro: GPIO8 + GPIO7 power, GRB. boards.h HAS_STATUS_PIXEL (combo + esp32-classic), platformPixelWrite() shim, /api/v2/device/info status_pixel_gpio, API docs and LED tables in the EN/NL manuals. Commit b1e8dd273 (alpha.420).

Evidence per AC:
- AC1: Classic + LOLIN S3 Mini (COM8, .88.63), app-only flash of combo alpha.420: status_pixel_gpio 47, psram_found 1 (ESP32-S3FH4R2), PIC 6.8; maintainer observed green (connected), red (boot before WiFi) and purple (AP); 0 settings changed.
- AC2: colour scheme from the maintainer before implementation.
- AC3: build.bat esp32-combo and esp32-classic 'Successfully created ESP32S3 image'; evaluate.py --quick 0 failed / 0 warnings.
- AC4: checked on maintainer instruction, NOT bench-tested (no Pro available); based on the core variant file.
- AC5: bench OTGW32 (psram_found 0, ESP32-S3FN4): status_pixel_gpio 0, OT-Direct and MQTT up, 0/182 settings changed.
Evidence files: %LOCALAPPDATA%/OTGW-capture/otgw32-validation-20261012/ (12_, 13_, 14_, 15_).
<!-- SECTION:FINAL_SUMMARY:END -->
