---
id: TASK-865.2
title: >-
  refactor(platform): drop ESP8266 source code paths and platform headers from
  2.0.0
status: To Do
assignee: []
created_date: '2026-06-13 05:41'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-128, drop-esp8266 source-side; depends on seq1)
Remove ESP8266 code paths/headers behind the platform abstraction. App-code surface is nearly empty: exactly ONE raw ESP8266 site left (`helperStuff.ino:398`); zero raw `#if ESP32` in app code. Mechanical collapse inside allowlisted abstraction files.

## Do NOT
Do not touch the cooperative loop (FreeRTOS is later phases). Do not swap libs. Do not touch vendored `src/libraries/SimpleTelnet/**` or `src/libraries/OTGWSerial/OTGWSerial.cpp` (ESP_ABSTRACTION_EXCLUDED_LIB_DIRS; their internal `#if ESP8266` stays byte-identical).

## Coupled deletions
- delete `src/libraries/Platform/src/platform_esp8266.h` + the `helperStuff.ino:398` `#ifdef ESP8266` g_platformMinFreeHeapWatermark block (ESP32 uses ESP.getMinFreeHeap()).
- `platform.h`: collapse dispatcher (22-28) + `PlatformDir` ESP8266 branches (53-119) to ESP32-only; `PLATFORM_INT_DISTINCT_FROM_INT32`->const 1 (KEEP jsonStuff.ino int/int32 overloads — int32_t is long on ESP32).
- `boards.h`: remove `BOARD_NODOSHOP_ESP8266` block (50-105) + esp8266 auto-detect (42-43) + the macro from `#error` (263).
- `OTGW-ModUpdateServer.h`: collapse `#if ESP8266` template (23-103) to `#include "OTGW-ModUpdateServer-esp32.h"`.
- DELETE `OTGW-ModUpdateServer-impl.h` (ESP8266 template impl).
- evaluate.py: remove platform_esp8266.h from ESP_ABSTRACTION_ALLOWED_FILES (514); lower ESP_ABSTRACTION_BASELINE 1->0 (557).
seq1 owns the platformio.ini/build.py esp8266 deletions; do NOT re-touch those here.

## Acceptance Criteria
- build: `python build.py --target esp32|esp32-classic|esp32-combo` exit 0 (grep per-env SUCCESS).
- evaluator: `evaluate.py` green; `scan_esp_abstraction_violations` == [] and BASELINE==0.
- grep: zero `BOARD_NODOSHOP_ESP8266`, zero `#if(def)/#elif ESP8266` outside SimpleTelnet/OTGWSerial; platform_esp8266.h + OTGW-ModUpdateServer-impl.h gone.
- grep: SimpleTelnet/** + OTGWSerial.cpp byte-identical; jsonStuff int/int32 overloads retained.
- field: flash esp32 (OTGW32) + esp32-classic; boot + web UI + live OT traffic.
<!-- SECTION:DESCRIPTION:END -->
