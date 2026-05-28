---
id: TASK-744
title: 'ESP abstraction Tier 4: OLED FreeRTOS-coupling refactor behind platform shims'
status: To Do
assignee: []
created_date: '2026-05-28 08:29'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 71000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OLED.ino currently references QueueHandle_t, xQueueCreate, xQueueReceive, IRAM_ATTR, gpio_set_intr_type, esp_timer_get_time etc. directly. Today this compiles only because HAS_OLED_CAPABLE is ESP32-only - but the inner #if defined(ESP32) checks at lines 42, 81, 91, 425, 452 contradict the abstraction principle. Introduce: PlatformOledButtonQueue type alias in platform_*.h; platformOledInitButton(pin, isrFn) (real impl on ESP32, simple pinMode on ESP8266); platformOledDrainButton(int64_t&) (real impl on ESP32, returns false on ESP8266). Move the ISR definition itself out of OLED.ino into platform_esp32.h (or a new platform_oled_esp32 sidecar). Depends on Tier 3.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OLED.ino contains no #if defined(ESP32) blocks - all platform-specific button/ISR/queue logic dispatches through platform shims
- [ ] #2 platform_esp32.h owns the ISR definition and FreeRTOS queue ops
- [ ] #3 platform_esp8266.h provides safe no-op equivalents (returning false / pinMode fallback)
- [ ] #4 OLED.ino:42 ESP32-specific includes are gone (moved into platform_esp32.h)
- [ ] #5 python build.py --firmware exits 0
<!-- AC:END -->
