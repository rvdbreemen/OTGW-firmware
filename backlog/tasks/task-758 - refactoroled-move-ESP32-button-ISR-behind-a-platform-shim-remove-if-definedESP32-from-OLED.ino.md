---
id: TASK-758
title: >-
  refactor(oled): move ESP32 button ISR behind a platform shim (remove #if
  defined(ESP32) from OLED.ino)
status: To Do
assignee: []
created_date: '2026-05-29 10:10'
labels: []
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-757/ADR-114. OLED.ino still branches the page-cycle button on raw #if defined(ESP32) (ESP-IDF gpio ISR + FreeRTOS queue) vs a non-ESP32 pinMode-only fallback (button does not cycle on ESP8266). This is a pre-existing ESP-abstraction violation (OLED.ino is not allowlisted). Move the button input behind a platform shim in platform_esp32.h / platform_esp8266.h (ESP8266 impl: debounced polling in loopOLED, or attachInterrupt) so OLED.ino carries no raw platform conditional and the button cycles pages on both platforms. Lower ESP_ABSTRACTION_BASELINE accordingly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OLED.ino contains no raw #if defined(ESP32)/ESP8266
- [ ] #2 button page-cycle works on both ESP32 and ESP8266
- [ ] #3 ESP_ABSTRACTION_BASELINE lowered in evaluate.py by the number of removed violations
<!-- AC:END -->
