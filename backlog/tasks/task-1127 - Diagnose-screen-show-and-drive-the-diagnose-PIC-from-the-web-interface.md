---
id: TASK-1127
title: 'Diagnose screen: show and drive the diagnose PIC from the web interface'
status: To Do
assignee: []
created_date: '2026-09-05 07:30'
labels:
  - feature
dependencies: []
priority: high
ordinal: 215000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer request, 2026-09-05: when the PIC runs diagnose firmware, the web interface must offer a dedicated screen that shows the diagnose output and lets you interact with it, so no extra tool (OTmonitor, telnet) is needed. The screen appears automatically once diagnose firmware is detected.\n\nDetection already exists and needs nothing new: OTGWSerial::matchBanner() sets the typed firmware enum plus version and processor from the PIC banner, for all three firmware types. Verified live on the bench unit, which reports picfwtype diagnose, picfwversion 2.2, picdeviceid pic16f1847. Use OTGWSerial.firmwareType(), not the state.pic.sType string projection.\n\nByte transparency on port 25238 already ships (TASK-1109, v1.7.5), so the menu and the terminator-less prompt reach a connected client today. What is missing is the browser path: raw PIC output into the page, and single keystrokes back out.\n\nDesign basis: docs/plan/DIAGNOSE_FIRMWARE_SUPPORT_PLAN.md stages 1 to 3. Key constraints from that analysis: the command queue cannot carry a keystroke because sendOTGW() appends CR+LF below every validator, so keystrokes need their own direct-to-UART writer; and the passthrough must keep reading through OTGWSerial::read() so matchBanner() still runs, otherwise the ESP never notices when the user reflashes gateway.hex.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A dedicated diagnose screen exists in the web interface as its own page section, and does not disturb the existing sections
- [ ] #2 The screen appears automatically when the PIC is identified as diagnose firmware, and is absent for gateway and interface firmware
- [ ] #3 Raw PIC output, including a prompt that carries no line terminator, is visible in the browser without any external tool
- [ ] #4 Single keystrokes can be sent from the browser to the PIC without a trailing CR or LF being appended, and Enter can be sent as its own explicit key
- [ ] #5 The ESP still detects a firmware type change, so reflashing gateway.hex from the same screen returns the device to normal operation without a reboot
- [ ] #6 python build.py exits 0 with a per-target success line, python evaluate.py --quick shows no new failures, and the screen is verified on the bench unit running diagnose 2.2
<!-- AC:END -->
