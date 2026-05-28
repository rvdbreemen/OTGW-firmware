---
id: TASK-740
title: >-
  ESP abstraction Tier 0: add HAS_SAT_BLE, HAS_WEATHER_FORECAST, HAS_SAT feature
  flags to boards.h
status: To Do
assignee: []
created_date: '2026-05-28 08:28'
labels:
  - esp-abstraction-audit
  - refactor
dependencies: []
ordinal: 67000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Infrastructure work that all higher tiers depend on. Add three feature flags in boards.h: HAS_SAT (1 on ESP32 boards, 0 on ESP8266), HAS_SAT_BLE (1 only when BLE hardware present), HAS_WEATHER_FORECAST (1 when forecast feature should be compiled in; default to current ESP32-only behaviour). No application-code changes in this tier - just the flags and short comments explaining each.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 boards.h defines HAS_SAT for every supported BOARD_NODOSHOP_* with the correct value
- [ ] #2 boards.h defines HAS_SAT_BLE for every supported board
- [ ] #3 boards.h defines HAS_WEATHER_FORECAST for every supported board
- [ ] #4 Each flag is documented inline with a one-line comment naming its rationale
- [ ] #5 python build.py --firmware passes on both ESP8266 and ESP32 board configs (no functional change yet, just new macros)
- [ ] #6 References docs/audits/esp-abstraction-leak-audit-YYYY-MM-DD.md section Tier 0
<!-- AC:END -->
