---
id: TASK-848
title: 'feat(combo): persist boot hardware-detection result to LittleFS log'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-09 21:49'
updated_date: '2026-06-09 22:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR-125 combo board logs its PIC/OTDirect boot-detection result via log_e to the USB/IDF console only (OTGW-firmware.ino:183). After a reboot that record is gone, and field testers without a USB console attached cannot tell what the board detected on first boot. Persist the same detection line to a capped rolling log file on LittleFS (/bootdetect.log) so it can be read after the fact via FSexplorer / the web file browser.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On each boot under HAS_RUNTIME_HW_DETECT, a plain-text line capturing the detection result (boot#, millis, eMode, picEnabled, boardMode, RST/RX/TX/I2C pins) is appended to /bootdetect.log on LittleFS
- [x] #2 The file is bounded: it never exceeds ~30 lines / a fixed byte cap; oldest lines are trimmed so LittleFS cannot fill across many reboots
- [x] #3 Log line format mirrors the existing log_e console line so console and file agree
- [x] #4 Implementation uses LittleFS + char[]/snprintf_P/PSTR per project rules; no String in the write path, all literals in flash
- [x] #5 File is readable via the existing FSexplorer / web file browser without a new endpoint
- [x] #6 Build green (python build.py) and evaluate.py --quick shows no new failures
- [ ] #7 FIELD: on real combo hardware, /bootdetect.log shows the correct detection result after a power cycle
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Impl: appendBootDetectLog() in OTGW-firmware.ino (HAS_RUNTIME_HW_DETECT guard), called right after the log_e detection line. Writes /bootdetect.log newest-first, temp-swap trim at 30 lines (mirrors updateRebootLog). boot# parsed from prior newest entry, no extra counter. Build: esp8266+esp32 green via build.py; esp32-combo green via penv pio (build.py does NOT target the combo env). Fixed sscanf_P->sscanf (not defined on ESP32; bug-035). evaluate.py --quick: 0 failed, 97.1%. Committed c3df1160 under alpha.170. AC7 (field power-cycle on real combo hw) remains for field validation.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Persist combo boot hardware-detection result to /bootdetect.log on LittleFS so it can be read via FSexplorer after a power cycle when no USB/IDF console is attached. Capped rolling log (30 lines, newest-first, temp-swap trim), plain-text lines mirroring the existing log_e console format, monotonic boot# parsed from the file. Also adds docs/hardware/PINOUT.md (Classic+PIC / OTGW32 / combo pin maps). ESP-abstraction clean (HAS_RUNTIME_HW_DETECT flag, no raw ifdef). Field AC open pending real combo hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
