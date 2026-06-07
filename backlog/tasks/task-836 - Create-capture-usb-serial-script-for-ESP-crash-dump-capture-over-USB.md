---
id: TASK-836
title: Create capture-usb-serial script for ESP crash-dump capture over USB
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 19:56'
updated_date: '2026-06-07 09:31'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The REST crash-log poller (TASK-835) captures the faulting address (epc1/excvaddr) that survives reboot, but NOT the full stack trace. The ESP8266 SDK panic handler prints the complete crash dump (exception cause + registers + >>>stack>>> trace) to UART0 at crash time, at the app Serial baud (115200). The firmware never persists that stack to flash, so the only way to get the call chain for addr2line is a live USB serial capture. Build a standalone single-file launcher (same shape as capture-mqtt-debug.bat) that logs UART0 continuously and flags crash frames, so George can capture the stack trace for the 1.7.0 crash-loop investigation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Standalone single-file .bat launcher with embedded PowerShell payload, same pattern as capture-mqtt-debug.bat (Q-to-stop, CancelFlag, merged transcript)
- [x] #2 Auto-detects the OTGW COM port via WMI friendly-name match (CP210x/CH340/USB Serial), with -Port COMn override
- [x] #3 Opens the port at 115200 by default with a -Baud override and an optional 74880 boot-ROM pass for the reset-cause banner
- [x] #4 Logs all UART0 bytes timestamped to usb-serial.log; flags crash frames (Exception (, epc1=, >>>stack>>>...<<<stack<<<, rst cause) into a separate crash-frames.log
- [x] #5 Gracefully handles port-in-use / device-absent / unplug-replug without aborting the capture
- [x] #6 Help text documents the exclusive-COM-port caveat (OTmonitor/flashing cannot share the port during capture) and the addr2line decode step needing the matching .elf
- [ ] #7 Optional: shell out to esptool esp_exception_decoder when toolchain + .elf are present
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added scripts/capture-usb-serial.bat (commit 5eaf21bc): single-file .bat launcher with embedded PowerShell that captures the ESP8266 UART0 stream over USB and mirrors the SDK panic dump (Fatal exception, epc1/epc2/epc3/excvaddr/depc, >>>stack>>>...<<<stack<<<) into a separate crash-frames.log. This is the fallback for pinning the crash when the network crashlog endpoint (TASK-835) gives a register-only decode that is not conclusive, since the full stack trace only reaches UART0 and is never persisted to flash.

Features: friendly-name COM auto-detect (CP210x/CH340/FTDI/Silicon Labs/Prolific) with -Port override; 115200 default with -Baud override; survives port-in-use and unplug/replug via a reopen loop; Q/Ctrl+C cooperative stop; -DurationSeconds; PowerShell 5.1 compatible.

Verified locally: payload parses clean (251 lines, 0 errors); --help renders; graceful open-failure + duration stop creates both logs and exits 0; crash-frame regex matches real ESP dump lines (Fatal exception / epc1 / stack block / ets-rst-cause / Soft WDT) with no false positive on normal log output.

Deviations from the original AC list, all intentional: (1) writes two logs (usb-serial.log raw + crash-frames.log) rather than one merged transcript, which is more useful for a serial capture; (2) 74880 boot-ROM capture is via -Baud override + documented in --help rather than an auto dual-baud switch (KISS, avoids fragile timing); (3) AC7 esptool esp_exception_decoder shell-out NOT implemented (optional) - addr2line decode is documented in --help and the stop banner instead.
<!-- SECTION:FINAL_SUMMARY:END -->
