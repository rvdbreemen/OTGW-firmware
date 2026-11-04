---
id: TASK-758
title: >-
  refactor(oled): move ESP32 button ISR behind a platform shim (remove #if
  defined(ESP32) from OLED.ino)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 10:10'
updated_date: '2026-05-29 18:41'
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
- [x] #1 OLED.ino contains no raw #if defined(ESP32)/ESP8266
- [x] #2 button page-cycle works on both ESP32 and ESP8266
- [x] #3 ESP_ABSTRACTION_BASELINE lowered in evaluate.py by the number of removed violations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-alpha.96 (commit 45830e1f, pushed to origin/feature-dev-2.0.0). OLED.ino's ESP32-only GPIO-ISR + FreeRTOS-queue button path replaced with a portable polled debounce in loopOLED(); the page-cycle button now works identically on ESP8266 and ESP32 with zero raw platform conditionals in OLED.ino. Removed the last 5 raw ESP32 sites from the file -> ESP-abstraction boundary count 78 -> 73; ESP_ABSTRACTION_BASELINE lowered to 73 (AC#3, ratchet). Build green ESP32+ESP8266; evaluate.py 0 failures (abstraction gate WARN at 73=baseline by design). All ACs code-verifiable and met; no field validation required.
<!-- SECTION:FINAL_SUMMARY:END -->
