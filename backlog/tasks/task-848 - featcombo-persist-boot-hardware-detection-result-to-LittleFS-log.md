---
id: TASK-848
title: 'feat(combo): persist boot hardware-detection result to LittleFS log'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-09 21:49'
updated_date: '2026-06-09 21:49'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR-125 combo board logs its PIC/OTDirect boot-detection result via log_e to the USB/IDF console only (OTGW-firmware.ino:183). After a reboot that record is gone, and field testers without a USB console attached cannot tell what the board detected on first boot. Persist the same detection line to a capped rolling log file on LittleFS (/bootdetect.log) so it can be read after the fact via FSexplorer / the web file browser.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On each boot under HAS_RUNTIME_HW_DETECT, a plain-text line capturing the detection result (boot#, millis, eMode, picEnabled, boardMode, RST/RX/TX/I2C pins) is appended to /bootdetect.log on LittleFS
- [ ] #2 The file is bounded: it never exceeds ~30 lines / a fixed byte cap; oldest lines are trimmed so LittleFS cannot fill across many reboots
- [ ] #3 Log line format mirrors the existing log_e console line so console and file agree
- [ ] #4 Implementation uses LittleFS + char[]/snprintf_P/PSTR per project rules; no String in the write path, all literals in flash
- [ ] #5 File is readable via the existing FSexplorer / web file browser without a new endpoint
- [ ] #6 Build green (python build.py) and evaluate.py --quick shows no new failures
- [ ] #7 FIELD: on real combo hardware, /bootdetect.log shows the correct detection result after a power cycle
<!-- AC:END -->
