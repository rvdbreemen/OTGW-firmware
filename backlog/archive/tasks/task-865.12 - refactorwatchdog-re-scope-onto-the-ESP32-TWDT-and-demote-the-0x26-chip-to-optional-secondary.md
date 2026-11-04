---
id: TASK-865.12
title: >-
  refactor(watchdog): re-scope onto the ESP32 TWDT and demote the 0x26 chip to
  optional secondary
status: Done
assignee:
  - '@claude'
created_date: '2026-06-13 05:56'
updated_date: '2026-06-21 07:07'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed TWDT re-scope (ADR-135): three watchdog shapes collapsed to two. #if HAS_PIC_WATCHDOG (esp32-classic + combo) now arms the ESP32 TWDT as PRIMARY with the external 0x26 I2C chip demoted to optional SECONDARY via secondaryWatchdogActive() = (!HAS_RUNTIME_HW_DETECT) || isPICEnabled(); #else (OTGW32) is TWDT-only. The retired esp8266-only external-only (no-TWDT) branch is gone (grep: 0 matches for 'HAS_PIC_WATCHDOG && !HAS_RUNTIME_HW_DETECT'). WatchDogEnabled() arms/disarms 0x26 on PCB boards, no-op on OTGW32. OTA per-chunk feed re-pointed from the deleted OTGW-ModUpdateServer-impl.h to OTGW-ModUpdateServer-esp32.h async handler; loop-task TWDT needs no reset there (loop keeps feeding via doBackgroundTasks). TWDT subscribes the loop task only (explicit decision; PIC-UART task deliberately not subscribed, residual gap recorded in ADR-135 Alternative 3). VERIFIED: esp32 + esp32-classic builds exit 0 (per-env [SUCCESS] grep-confirmed, not just wrapper exit); evaluate.py --quick 0 failures (1 pre-existing boards.h-path warning, present on clean tree). REMAINING (field-validation, blocks Done): (a) induced loop hang triggers TWDT panic-reset within timeout on esp32 + esp32-classic; (b) on esp32-classic the 0x26 chip does NOT spuriously reset in normal operation; (c) OTA + PIC-flash complete without a watchdog reset on both. SEPARATE: ADR-135 acceptance is pending maintainer ratification (Proposed; accepted in a later docs(adr) commit per repo convention, like ADR-131..134).

Follow-up fix (maintainer dispositie 2026-06-14): made the combo external-0x26 feed + boot-read UNCONDITIONAL, matching the already-unconditional arm. Removed secondaryWatchdogActive() and its isPICEnabled() gate. Rationale: 0x26 presence is not reliably detectable (later NodoShop Classic revisions dropped the chip; the only PIC probe is detectPIC() reset->ETX), feeding an absent 0x26 is a harmless NACK, while the previous arm-unconditional/feed-gated asymmetry could arm-but-not-feed a present chip on a combo+old-Classic-PCB with a dead/undetected PIC -> spurious reset loop. ADR-135 Decision/Risk updated. BUILD: esp32 + esp32-classic SUCCESS; esp32-combo links clean but fails the PRE-EXISTING partition-size check (101%, 1986399/1966080 B), unrelated to this change. EVAL: evaluate.py --quick 0 failures (98.6%).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Re-scoped the watchdog onto the ESP32 TWDT and demoted the external 0x26 chip to optional secondary (ADR-135). Live on dev. Closed per migration-accepted sign-off (Robert 2026-06-21).
<!-- SECTION:FINAL_SUMMARY:END -->
