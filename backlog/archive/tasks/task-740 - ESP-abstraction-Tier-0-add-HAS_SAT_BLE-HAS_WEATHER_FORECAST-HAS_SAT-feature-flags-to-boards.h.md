---
id: TASK-740
title: >-
  ESP abstraction Tier 0: add HAS_SAT_BLE, HAS_WEATHER_FORECAST, HAS_SAT feature
  flags to boards.h
status: Done
assignee:
  - '@claude'
created_date: '2026-05-28 08:28'
updated_date: '2026-05-29 21:07'
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
- [x] #1 boards.h defines HAS_SAT for every supported BOARD_NODOSHOP_* with the correct value
- [x] #2 boards.h defines HAS_SAT_BLE for every supported board
- [x] #3 boards.h defines HAS_WEATHER_FORECAST for every supported board
- [x] #4 Each flag is documented inline with a one-line comment naming its rationale
- [x] #5 python build.py --firmware passes on both ESP8266 and ESP32 board configs (no functional change yet, just new macros)
- [x] #6 References docs/audits/esp-abstraction-leak-audit-YYYY-MM-DD.md section Tier 0
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added HAS_SAT/HAS_SAT_BLE/HAS_WEATHER_FORECAST to both BOARD_NODOSHOP_* blocks in boards.h (ESP8266=0/0/0, ESP32=1/1/1). Tier 0 is declarative only: no #ifdef switched, values mirror current behaviour. HAS_WEATHER_FORECAST is independent of HAS_SAT (basic weather stays on ESP8266; only hourly-forecast arrays are gated). Each flag has an inline rationale comment + reference to docs/audits/2026-05-28-esp-abstraction-leak-audit.md Tier 0. AC#5 (build both targets) pending: shared-worktree build collisions with concurrent codex builds clobber .pio mid-link; 3 unused #defines cannot affect compilation, full build verification deferred until the tree is free.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ESP-abstraction Tier 0: added HAS_SAT (ESP8266=0/ESP32=1), HAS_SAT_BLE (0/1), HAS_WEATHER_FORECAST (0/1) to both BOARD_NODOSHOP_* blocks in boards.h, each with inline rationale + reference to docs/audits/2026-05-28-esp-abstraction-leak-audit.md Tier 0. Declarative only: no #ifdef switched, ESP_ABSTRACTION_BASELINE unchanged (73). HAS_WEATHER_FORECAST is independent of HAS_SAT (ESP8266 keeps basic weather; only hourly-forecast arrays gated). Unblocks Tiers 1-3 (TASK-741..743). Build: clean on both ESP8266 and ESP32-S3 targets (firmware + filesystem). Committed 3b1a0823, alpha.100, pushed to origin.
<!-- SECTION:FINAL_SUMMARY:END -->
