---
id: TASK-865.12
title: >-
  refactor(watchdog): re-scope onto the ESP32 TWDT and demote the 0x26 chip to
  optional secondary
status: To Do
assignee: []
created_date: '2026-06-13 05:56'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.2
  - TASK-865.6
parent_task_id: TASK-865
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 4; watchdog; depends seq2=TASK-865.2, seq6=TASK-865.6)
Re-scope the watchdog (ADR-011) onto the ESP32 TWDT and drop the external-hardware-watchdog dependency. The ESP32 TWDT is ALREADY wired (OTGW-Core.ino watchdog block ~840-1034, three shapes dispatched by HAS_PIC_WATCHDOG/HAS_RUNTIME_HW_DETECT, ADR-127). Work = (a) finish the re-scope so TWDT is primary on every survivor, (b) decide the 0x26 feed's fate now ESP8266 is gone. The external chip stays on esp32-classic/combo-when-PIC (boards.h HAS_PIC_WATCHDOG 1 for BOARD_NODOSHOP_ESP32_CLASSIC ~231; OTGW Classic PCB carries 0x26). Outcome: TWDT primary all ESP32-S3 builds; 0x26 optional secondary on classic/combo-PIC, absent on OTGW32 - exactly the combo branch (OTGW-Core.ino:913-984).

## What
1. After esp8266 removal, collapse to two surviving shapes; the HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT esp8266-only branch goes away/merges. CONFIRM esp32-classic retains a working TWDT: today it hits the !HAS_RUNTIME_HW_DETECT external-only branch with NO esp_task_wdt_* calls; that branch must gain a TWDT or route to the combo-shaped code.
2. Keep WatchDogEnabled() semantics: external arm/disarm on 0x26 boards (OTA flash disarm), no-op on OTGW32 (1019-1022). TWDT has no enable/disable -> a long flash chunk must esp_task_wdt_reset() or temporarily delete the subscription.
3. Couple the TWDT subscription set to the seq6 PIC task: today esp_task_wdt_add(NULL) subscribes only the loop task (938/1014); subscribe the safety-critical PIC task too (or record an explicit decision to watch only loop).
4. Write a superseding/amending ADR (2.0.0 numbering) for ADR-011: TWDT primary, 0x26 optional-secondary, ESP8266 single-failure-domain rationale retired; cross-ref ADR-123/127.
Note: OTGW-ModUpdateServer-impl.h (flash-feed ~206) is DELETED by seq2; the ESP32 flash-feed lives in OTGW-ModUpdateServer-esp32.h / the seq11 async OTA handler - re-point there.

## Acceptance Criteria
- build: esp32 + esp32-classic exit 0 after the re-scope.
- evaluator: evaluate.py --quick no new failures; no raw #ifdef ESP8266/board-macro reads outside allowlisted files; grep confirms esp32-classic reaches an esp_task_wdt_* path (TWDT armed), the esp8266-only external-only (no-TWDT) branch removed/merged, no shape lacks esp_task_wdt_reset().
- build: WatchDogEnabled() no-op on OTGW32, drives 0x26 arm/disarm on PCB boards; OTA flash path compiles + disarm/feed preserved (re-pointed to the ESP32 async OTA handler).
- A superseding/amending ADR (2.0.0 numbering) for ADR-011 is Accepted: TWDT-primary + optional-secondary-0x26 + retired single-failure-domain rationale, cross-ref ADR-123/127.
- field: on esp32 (OTGW32) + esp32-classic, an induced loop hang triggers a TWDT panic-reset within timeout; on esp32-classic the 0x26 chip does NOT spuriously reset in normal operation; OTA + PIC-flash complete without a watchdog reset on both.
<!-- SECTION:DESCRIPTION:END -->
