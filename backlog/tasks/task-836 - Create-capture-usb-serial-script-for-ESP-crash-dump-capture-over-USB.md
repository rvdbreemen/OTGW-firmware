---
id: TASK-836
title: Create capture-usb-serial script for ESP crash-dump capture over USB
status: To Do
assignee: []
created_date: '2026-06-06 19:56'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The REST crash-log poller (TASK-835) captures the faulting address (epc1/excvaddr) that survives reboot, but NOT the full stack trace. The ESP8266 SDK panic handler prints the complete crash dump (exception cause + registers + >>>stack>>> trace) to UART0 at crash time, at the app Serial baud (115200). The firmware never persists that stack to flash, so the only way to get the call chain for addr2line is a live USB serial capture. Build a standalone single-file launcher (same shape as capture-mqtt-debug.bat) that logs UART0 continuously and flags crash frames, so George can capture the stack trace for the 1.7.0 crash-loop investigation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Standalone single-file .bat launcher with embedded PowerShell payload, same pattern as capture-mqtt-debug.bat (Q-to-stop, CancelFlag, merged transcript)
- [ ] #2 Auto-detects the OTGW COM port via WMI friendly-name match (CP210x/CH340/USB Serial), with -Port COMn override
- [ ] #3 Opens the port at 115200 by default with a -Baud override and an optional 74880 boot-ROM pass for the reset-cause banner
- [ ] #4 Logs all UART0 bytes timestamped to usb-serial.log; flags crash frames (Exception (, epc1=, >>>stack>>>...<<<stack<<<, rst cause) into a separate crash-frames.log
- [ ] #5 Gracefully handles port-in-use / device-absent / unplug-replug without aborting the capture
- [ ] #6 Help text documents the exclusive-COM-port caveat (OTmonitor/flashing cannot share the port during capture) and the addr2line decode step needing the matching .elf
- [ ] #7 Optional: shell out to esptool esp_exception_decoder when toolchain + .elf are present
<!-- AC:END -->
