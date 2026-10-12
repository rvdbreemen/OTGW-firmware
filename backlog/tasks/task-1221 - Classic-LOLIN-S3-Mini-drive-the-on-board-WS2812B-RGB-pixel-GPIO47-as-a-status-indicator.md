---
id: TASK-1221
title: >-
  Classic + LOLIN S3 Mini: drive the on-board WS2812B RGB pixel (GPIO47) as a
  status indicator
status: To Do
assignee: []
created_date: '2026-10-12 16:50'
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
- [ ] #1 Pixel shows the agreed status colours on an OTGW Classic + LOLIN S3 Mini in PIC mode (bench photo or screenshot)
- [ ] #2 OTGW32 never drives GPIO47 as a pixel; Classic in OT-Direct fallback does not toggle GPIO47 as the bypass relay
- [ ] #3 Colour scheme agreed with the maintainer before implementation
- [ ] #4 build.bat --target esp32-combo and esp32-classic green, evaluate.py --quick no new FAIL
<!-- AC:END -->
