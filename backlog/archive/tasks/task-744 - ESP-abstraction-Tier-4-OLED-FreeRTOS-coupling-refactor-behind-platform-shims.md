---
id: TASK-744
title: 'ESP abstraction Tier 4: OLED FreeRTOS-coupling refactor behind platform shims'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-28 08:29'
updated_date: '2026-06-01 19:42'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T21:42:12+02:00: PREMISE OBSOLETE — closing as already-satisfied, no code change. 744 was written to move OLED.ino's ESP32 button ISR (QueueHandle_t/xQueueCreate/xQueueReceive/IRAM_ATTR/gpio_set_intr_type) behind platformOledInitButton/platformOledDrainButton shims. VERIFIED current state: that machinery NO LONGER EXISTS — TASK-758 (Done) rewrote the OLED button as a POLLED input in loopOLED() (debounce via oledLastBtnLevel/oledLastBtnEdge, OLED_DEBOUNCE_MS), no interrupt, no FreeRTOS queue. grep of OLED.ino: 0 raw #if defined(ESP8266/ESP32); only HAS_ETH_CAPABLE / HAS_DIRECT_OT capability flags + _VERSION_PRERELEASE remain (all legitimate). The only IRAM_ATTR in the tree are OTDirect OT-bus ISRs + s0 pulse counter, unrelated to OLED. No platformOled* shim needed because there is no ISR/queue to abstract. 744's abstraction goal is met by 758's polled rewrite. Closing Done (premise superseded). Note: does not affect ESP_ABSTRACTION_BASELINE (no OLED sites were ever in the count post-758).
<!-- SECTION:NOTES:END -->
