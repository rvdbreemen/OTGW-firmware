---
id: TASK-848
title: 'feat(combo): persist boot hardware-detection result to LittleFS log'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-09 21:49'
updated_date: '2026-10-12 00:04'
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

2026-10-12 Classic-S3 bench validation (alpha.418 combo on OTGW Classic + LOLIN S3 Mini, PIC16F1847 gw 6.8): Bench evidence 2026-10-11/12 (C:/Users/rvdbr/AppData/Local/OTGW-capture/classic-validation-20261011/EVIDENCE.md), OTGW Classic carrier + LOLIN S3 Mini (MAC ac:27:6e:ce:45:d8), PIC16F1847 gateway 6.8, esp32-combo 2.0.0-alpha.418+fa2b89d. After the fs flash and 2 RTS resets, /bootdetect.log held boot#1 t=2636ms, boot#2 t=2950ms, boot#3 t=2623ms. Each line read `eMode=1 pic=1 mode=1 pro=0 RST=12 RX=44 TX=43 I2C=35/36`, which is the correct PIC detection and the Classic pin map. Board-mode reboots (POST boardmode + /ReBoot) went on as boot#5 (mode 1) and boot#6 (mode 2, corrected to 1 at boot), both `eMode=1 pic=1 mode=1`. The boot counter stays monotonic and survives reboots. AC#7 is partly evidenced, Classic half only. Still open: (a) a real power cycle, since every boot here was an RTS/USB-CDC chip reset or a software reboot; (b) the OT-Direct (OTGW32) half on combo, because no OTGW32 boot appears in this evidence. Side note from the same run: the console `[combo] detect` log_e line does not appear on USB-CDC because platformMuteUart0Console() also silences it (TASK-850/TASK-1217). /bootdetect.log is therefore currently the only place the detection result is recorded.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Persist combo boot hardware-detection result to /bootdetect.log on LittleFS so it can be read via FSexplorer after a power cycle when no USB/IDF console is attached. Capped rolling log (30 lines, newest-first, temp-swap trim), plain-text lines mirroring the existing log_e console format, monotonic boot# parsed from the file. Also adds docs/hardware/PINOUT.md (Classic+PIC / OTGW32 / combo pin maps). ESP-abstraction clean (HAS_RUNTIME_HW_DETECT flag, no raw ifdef). Field AC open pending real combo hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
