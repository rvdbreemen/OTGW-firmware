---
id: TASK-1127
title: 'Diagnose screen: show and drive the diagnose PIC from the web interface'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-05 07:30'
updated_date: '2026-09-05 07:33'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-05: measured the real diagnose menu on the bench unit (192.168.88.68, diagnose 2.2) over port 25238, which is byte-transparent since v1.7.5. Five of the design plan's open questions are now answered from hardware rather than from a single account, and one answer contradicts the brief.

1. THE MENU IS LINE-BASED, NOT PER-KEYSTROKE. Sending 1, then 4, then 6 without CR produced only the echoes "1", "4", "6" and no action; the following CR returned "Invalid test", because the PIC had buffered "146" and evaluated it as one number. The prompt "Enter test number: " reads a LINE. This is the opposite of what the brief assumed, and it means menu navigation does not need a single-keystroke channel at all.

2. INSIDE A RUNNING TEST, KEYS ARE CONSUMED SILENTLY. After "1" + CR the LED test starts and emits nothing. Subsequent "4" and "6" produced ZERO bytes back: no echo, no output. So the single-keystroke channel IS needed, but only while a test is running, and the user gets no feedback from the PIC for those keys.

3. THE PIC ECHOES AT THE MENU. "0" came back as "0". The browser must NOT add local echo at the menu or every character doubles. Inside a test there is no echo, so the UI has to represent those keypresses itself.

4. ENTER IS A BARE CR (0x0D). It both submits a menu choice and exits a running test. No LF needed.

5. THE BANNER IS REPRINTED ON EVERY MENU REDRAW, and that costs. Verified against the telnet debug log: each redraw re-fires fwreportinfo(), which calls sendMQTTversioninfo(). Two redraws in one 22 s window produced two full callback sequences. Every Enter in the diagnose screen would therefore publish MQTT version info. This needs throttling and it is firmware work beyond the screen itself.

6. Invalid input yields 0x04 (ETX) followed by "Invalid test" and a full banner+menu+prompt redraw. The 0x04 is the same byte detectPIC() probes for, which is consistent with the probe succeeding against diagnose.hex.

7. On connect the PIC sends NOTHING. It sits silently at its prompt. The screen therefore cannot rely on passively waiting for output; it needs to send a CR to provoke a redraw when it opens, or accept a blank pane until the user acts.
<!-- SECTION:NOTES:END -->
