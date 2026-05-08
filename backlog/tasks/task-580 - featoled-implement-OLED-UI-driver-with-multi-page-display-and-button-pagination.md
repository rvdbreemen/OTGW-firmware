---
id: TASK-580
title: >-
  feat(oled): implement OLED UI driver with multi-page display and button
  pagination
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 15:59'
updated_date: '2026-05-08 17:27'
labels:
  - ui
  - oled
  - hardware
  - otgw32
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 has a 5-page OLED display (SSD1306 128x64) with button-driven pagination and 30s auto-off timeout. The 2.0.0 firmware detects OLED presence (state.hw.bOLEDPresent) but renders nothing. Implement the display driver and UI pages so OTGW32 users with the Nodoshop NODO hardware have on-device status feedback without needing a browser.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OLED driver initialises correctly on I2C 0x3C if HAS_OLED_CAPABLE is defined and device is detected at boot
- [x] #2 At minimum 3 pages implemented: network status (IP, WiFi/Eth, MQTT connected), OT-Direct status (mode, boiler connected, flame), device status (version, uptime, heap)
- [x] #3 Boot button (GPIO 9) cycles pages on short press; display wakes from off state on button press
- [x] #4 Display auto-off after 30 seconds of inactivity
- [x] #5 OLED code is gated behind HAS_OLED_CAPABLE so it compiles cleanly on non-OLED builds
- [x] #6 No regression on ESP8266 PIC build (OLED code must not be compiled in)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented full 5-page OLED UI driver for OTGW32 (ESP32-S3, SSD1306 128x64 at 0x3C).

Pages:
1. Network: WiFi SSID or Ethernet, IP address, RSSI (WiFi only), MQTT connected/offline, broker hostname
2. OT Status: hardware mode (OT-Direct/PIC/Degraded), bus online/offline, flame+modulation, CH/DHW flags, setpoint+boiler temp
3. Device: firmware version, hostname, uptime (d/h/m), free heap, reboot count
4. Dashboard: flame/mod, CH/DHW, 6 key temperatures
5. Heating: HC1+HC2 setpoints, room temps, flow temps

Key fixes vs previous version:
- Compile guard: #if HAS_OLED_CAPABLE -> #if defined(HAS_OLED_CAPABLE) && HAS_OLED_CAPABLE (safe on ESP8266)
- Button: PIN_BUTTON (GPIO 0, boot) -> OLED_BUTTON_PIN resolved from PIN_CONFIG_BUTTON (GPIO 9)
- Wake-from-off: page==0 + press now goes directly to page 1 with DISPLAYON
- state.hw.bOLEDPresent = true set on successful probe in initOLED()
- oledWake() forces immediate redraw via oledLastRefresh = 0

Files: src/OTGW-firmware/OLED.ino
Build guard: HAS_OLED_CAPABLE (ESP8266 has no OLED, compiles out entirely)
Library: SSD1306Ascii 1.3.5 (text-only, no 1KB framebuffer needed)
<!-- SECTION:FINAL_SUMMARY:END -->
