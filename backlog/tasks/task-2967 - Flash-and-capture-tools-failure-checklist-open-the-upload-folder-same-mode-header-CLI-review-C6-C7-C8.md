---
id: TASK-2967
title: >-
  Flash and capture tools: failure checklist, open the upload folder, same mode
  header (CLI review C6 C7 C8)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:33:00Z'
labels:
  - tools
  - ux
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1099000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2963 CLI review (docs/reviews/2026-11-20_ux-cli-tools/README.md, C6, C7, C8). C6: a failed write_flash prints only '[ERROR] write_flash failed.' (flash_otgw.bat:235-237; .sh:576-580). C7: the capture prints the upload path deep under logs/mqtt-diagnostics (otgw_capture.py:4163-4171) and the tester has to find it by hand. C8: the .sh mode header leaves out the WiFi erase (flash_otgw.sh:216 vs flash_otgw.bat:87).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a failed write_flash both flash tools print a 4-step checklist: close other serial programs (OTmonitor, Arduino, a serial monitor), use a USB data cable, add --port when several boards are connected, try --baud 115200; with the platform's port example (COMx or /dev/ttyACM0)
- [ ] #2 After a successful interactive capture the tool offers to open the folder with the upload zip selected (explorer /select on Windows, open -R on macOS, xdg-open on Linux); --no-prompt runs are unchanged
- [ ] #3 The flash_otgw.sh factory header names the WiFi erase like the .bat
<!-- AC:END -->
