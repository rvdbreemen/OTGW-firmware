---
id: TASK-862
title: >-
  fix(esp32-classic): PIC UART pin binding via OTGWSerial constructor args
  (TU-visibility bug)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-12 04:40'
updated_date: '2026-06-13 05:05'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
PIC never detected on esp32-classic: OTGWSerial.cpp ESP32 branch guards UART pin choice with #if defined(PIN_PIC_RX)/(PIN_PIC_TX), but boards.h is outside that translation unit's include chain and no -D flags exist, so fallback begin(9600,8N1,16,17) compiled in (GPIO16=PIN_LED1, GPIO17 unconnected). Fix per maintainer decision 2026-06-12: OTGWSerial library MAY be modified (sole library exception). Extend constructor with rxPin/txPin parameters (HardwareSerial::begin style) so the application TU (where boards.h IS visible) passes PIN_PIC_RX/PIN_PIC_TX at instantiation. Root cause analysis: bug-119 in .wolf/buglog.json.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGWSerial constructor accepts rxPin/txPin (default -1 = legacy behaviour), ESP32 branch binds those pins when >= 0
- [x] #2 Application instantiation passes PIN_PIC_RX/PIN_PIC_TX from boards.h; all three boards.h sections define them (ESP8266: -1 sentinel)
- [x] #3 All three build targets compile clean (python build.py)
- [x] #4 evaluate.py --quick green, no new violations
- [x] #5 Field validation: PIC detected on LOLIN S3 Mini in OTGW Classic socket (user hardware test)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Extend OTGWSerial ctor with rxPin/txPin (HardwareSerial::begin style), default -1.
2. ESP32 branch: bind passed pins when both >= 0, else legacy 16/17 fallback; ESP8266 branch ignores (UART0 fixed). Dead compile-time PIN_PIC_RX/TX macro check removed from lib TU.
3. boards.h ESP8266 section: PIN_PIC_RX/TX = -1 sentinels so the shared instantiation compiles on all PIC boards.
4. OTGW-firmware.h: pass PIN_PIC_RX/PIN_PIC_TX at instantiation (app TU sees boards.h).
5. Build all targets + evaluate, bump, commit, push. Field validation by Robert on S3 Mini hardware.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented. Files: OTGWSerial.h:143 (signature), OTGWSerial.cpp:834-858 (ctor branches), boards.h ESP8266 section (+2 sentinel defines), OTGW-firmware.h:55-57 (instantiation passes pins). Maintainer granted OTGWSerial as the sole editable vendored lib (2026-06-12). evaluate.py --quick: 0 failed / 2 pre-existing warnings, health 97.1%. Root cause record: bug-119 in .wolf/buglog.json.

Build green all three targets after slug fix (TASK-863 unblocked the gate). Commits: 77f084b0 (fix + alpha.175 bump), pushed to origin. handleDebug.ino comment correction (stale ADR-125 reference) rode along in the bump commit. AC5 (field validation on S3 Mini hardware) remains open: flash alpha.175 esp32-classic and confirm PIC detect.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OTGWSerial constructor extended with rxPin/txPin parameters. Application TU passes PIN_PIC_RX/PIN_PIC_TX from boards.h. Field validated via combo build (alpha.176) on Classic PCB (S3 Mini): eMode=1 pic=1, PIC detected on GPIO43/44, full PR round-trips confirmed. The esp32-classic build carries identical fix (commit 77f084b0); pins/detection path are identical between esp32-classic and esp32-combo.
<!-- SECTION:FINAL_SUMMARY:END -->
