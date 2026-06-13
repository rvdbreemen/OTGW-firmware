---
id: TASK-865.4
title: >-
  feat(build): add async/FreeRTOS lib stack to ESP32 lib_deps with a gated
  link-proof smoke
status: To Do
assignee: []
created_date: '2026-06-13 05:42'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 foundation; SOLE owner of the async lib_deps)
Add the async stack to `[env:esp32].lib_deps` (esp32-classic/combo inherit via extends): ESP32Async/AsyncTCP (pin FIRST, lock the maintained fork), ESP32Async/ESPAsyncWebServer (incl. AsyncWebSocket), espMqttClient (bertmelis, plain non-async variant). seq7/9/10/11 are pure consumers and MUST NOT re-declare these.

## What
- platformio.ini:165-174 ([env:esp32] lib_deps, next to EthernetESP32/NimBLE) add the three with ADR-123 comments. Resolve exact version pins via `pio pkg search` + cross-check other-projects/EMS-ESP32-dev/platformio.ini (the blueprint); record pins in notes.
- gated link-proof smoke in `src/libraries/Platform/src/platform_esp32.h:19-29` (allowlisted include aggregator): reference one symbol per lib (e.g. `(void)&Sym`) so -flto does not dead-strip; NOT a bare include. Keep strippable to limit flash.
Independence: ESP32-gated, builds clean while esp8266 may still be in tree (could parallelize seq1-3).

## Acceptance Criteria
- build: `build.py --target esp32` exit 0, log shows AsyncTCP+ESPAsyncWebServer+espMqttClient resolved/linked; esp32-classic+esp32-combo also exit 0 (inherited deps link on all three).
- build: per-target flash usage captured; esp32-combo stays <100% of the 1.875MB app slot (ADR-127 ~98.4%, ~30KB headroom); if overflow, scope the combo smoke strippable or build link-proof only on esp32.
- build: smoke REFERENCES a symbol per lib (not dead-stripped under -flto; confirm in link map).
- evaluator: `evaluate.py --quick` no new failures; check_esp_abstraction_boundary green (includes in platform_esp32.h, no raw #ifdef ESP32 in .ino).
- chosen version pins recorded in task notes with source (pio pkg search + EMS-ESP32 platformio.ini).
<!-- SECTION:DESCRIPTION:END -->
