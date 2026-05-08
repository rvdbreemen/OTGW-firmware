---
id: TASK-580
title: >-
  feat(oled): implement OLED UI driver with multi-page display and button
  pagination
status: To Do
assignee: []
created_date: '2026-05-08 15:59'
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
- [ ] #1 OLED driver initialises correctly on I2C 0x3C if HAS_OLED_CAPABLE is defined and device is detected at boot
- [ ] #2 At minimum 3 pages implemented: network status (IP, WiFi/Eth, MQTT connected), OT-Direct status (mode, boiler connected, flame), device status (version, uptime, heap)
- [ ] #3 Boot button (GPIO 9) cycles pages on short press; display wakes from off state on button press
- [ ] #4 Display auto-off after 30 seconds of inactivity
- [ ] #5 OLED code is gated behind HAS_OLED_CAPABLE so it compiles cleanly on non-OLED builds
- [ ] #6 No regression on ESP8266 PIC build (OLED code must not be compiled in)
<!-- AC:END -->
