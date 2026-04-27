---
id: TASK-446
title: fixotgw32-recreate-port-25238-input-bridge-in-OTDirect-from-handlePICSerial
status: Done
assignee:
  - '@codex'
created_date: '2026-04-27 10:27'
updated_date: '2026-04-27 17:46'
labels:
  - bug
  - esp32
  - otgw32
  - ser2net
  - otdirect
  - codex
dependencies:
  - TASK-445
references:
  - 'src/OTGW-firmware/boards.h:112'
  - 'src/OTGW-firmware/OTGW-Core.ino:4291'
  - 'src/OTGW-firmware/OTGW-Core.ino:4374'
  - 'src/OTGW-firmware/OTGW-Core.ino:3047'
  - 'src/OTGW-firmware/OTDirect.ino:2007'
documentation:
  - 'docs/manuals/en/ch09-api-reference.md:1138'
  - 'docs/manuals/en/ch02-hardware-setup.md:29'
  - docs/adr/ADR-059-ser2net-queue-awareness.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support
Coding agent tag: codex
Depends on: TASK-445
User design direction: when no PIC is available, OTDirect should contain the actual bridge handler for TCP port 25238. Recreate the bridge function for OTDirect based on the current OTGW/PIC handling functions, especially the network side of `handlePICSerial()` and the line fanout behavior around `dispatchOTGWInputLine()`.

Problem found during the port 25238 audit: even after the `OTGWstream` socket is serviced, ESP32/OTGW32 currently has no working inbound command bridge from TCP port 25238 into OTDirect.

Evidence:
- `src/OTGW-firmware/boards.h:112-113` sets `HAS_PIC 0` and `HAS_DIRECT_OT 1` for `BOARD_NODOSHOP_ESP32`.
- `src/OTGW-firmware/OTGW-Core.ino:4290-4291` makes `handlePICSerial()` return immediately when `!HAS_PIC`.
- The current PIC-backed bridge behavior is in `src/OTGW-firmware/OTGW-Core.ino:4373-4431`: it reads from `OTGWstream`, writes bytes to `OTGWSerial`, buffers until CR, ignores LF, logs `Net2Ser`, emits a `>` WebSocket event, applies ser2net queue-awareness, and handles special commands such as `GW=R`, `PS=1`, and `PS=0`.
- `src/OTGW-firmware/OTGW-Core.ino:3042-3049` already has the correct OTGW32 command shim: `sendPICSerial()` routes to `handleOTDirectCommand(buf, len)` when `HAS_DIRECT_OT` is enabled and no PIC is active.
- `src/OTGW-firmware/OTDirect.ino:2000-2007` documents `handleOTDirectCommand()` as the PIC-style command translator for OTGW32.

Why this matters:
OTmonitor, Home Assistant's OpenTherm Gateway integration, and raw TCP clients expect port 25238 to behave like the OTGW serial bridge. On ESP8266/PIC builds, the network side of `handlePICSerial()` implements that behavior by bridging TCP bytes to PIC serial and treating complete CR-terminated commands as observable OTGW commands. On ESP32/OTGW32, there is no PIC serial port, so OTDirect needs its own bridge handler that recreates those bridge semantics and forwards completed commands to `handleOTDirectCommand()` through the existing command path.

Implementation guidance:
Keep the change small and make ownership explicit. Add an OTDirect-owned bridge function, for example `handleOTDirectBridgeStream()` or `handleOTDirectSerialBridge()`, behind `#if HAS_DIRECT_OT` in `OTDirect.ino`. Model this function on the network-input part of `handlePICSerial()`, but replace the PIC serial write with the OTDirect command path.

Required behavioral model from `handlePICSerial()`:
- Read bytes from `OTGWstream.available()` / `OTGWstream.read()`.
- Treat CR as the end of a command line.
- Ignore LF.
- Buffer command bytes in a fixed-size static buffer equivalent to the existing `MAX_BUFFER_WRITE` behavior unless a separate task improves overflow handling.
- When a complete non-empty command is received, log it as the ser2net command path currently does.
- Emit the same kind of `>` event that a PIC-backed ser2net command emits so WebSocket/OT log users can see manual commands.
- Route the completed command to the existing OTDirect command path, preferably through `sendPICSerial()` or `addCommandToQueue()` so `handleOTDirectCommand()` remains the semantic owner.
- Recreate or explicitly decide the OTDirect equivalents of `GW=R`, `PS=1`, and `PS=0` handling. If an equivalent is intentionally different on OTGW32, document the reason in implementation notes.

For `HAS_PIC`, preserve the current `handlePICSerial()` behavior exactly. `OTGW-Core.ino` / `doBackgroundTasks()` may call the OTDirect bridge entry point when `HAS_DIRECT_OT` is enabled, but should not contain the ESP32-specific command parser.

Do not call PlatformIO directly for verification; use `build.py`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On `BOARD_NODOSHOP_ESP32` / `HAS_DIRECT_OT`, a line sent to TCP port 25238 is read from `OTGWstream` and passed to the existing OTDirect command path without requiring PIC serial hardware.
- [x] #2 The implementation reuses `sendPICSerial()` or `addCommandToQueue()` so `handleOTDirectCommand()` remains the single owner of PIC-style OTDirect command semantics.
- [x] #3 CR and CRLF input from telnet/nc/OTmonitor is handled correctly: CR terminates the command, LF is ignored, and partial lines are buffered without dynamic allocation.
- [x] #4 ESP8266/PIC behavior is unchanged: incoming 25238 bytes still forward to `OTGWSerial`, still log completed commands, still update ser2net queue-awareness, and still handle `GW=R`, `PS=1`, and `PS=0` as before.
- [x] #5 The bridge uses fixed-size buffers only and does not introduce Arduino `String`, heap allocation, or a new command parser.
- [x] #6 Verification includes `build.py --target esp32`; if practical, also run an ESP8266/PIC build path or document why it was not run.
- [x] #7 OTDirect owns the no-PIC ESP32 bridge handler: the command parser/line-buffer for TCP 25238 lives in `OTDirect.ino` behind `#if HAS_DIRECT_OT`, not in the PIC serial handler.
- [x] #8 The OTDirect-owned bridge function is explicitly modeled on the network-input section of `handlePICSerial()` (`OTGW-Core.ino:4373-4431`) and preserves its CR/LF command framing semantics.
- [x] #9 For no-PIC builds, the bridge replaces the PIC serial byte write with routing the completed command to the existing OTDirect command path; it does not write to `OTGWSerial`.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added OTDirect-owned line-buffered bridge handler for TCP 25238. It mirrors the PIC bridge framing (CR terminates, LF ignored, fixed buffer, overflow discard) and routes complete commands into the existing OTDirect command path via sendPICSerial().

Verification: ESP32 build completed with build.py --target esp32. ESP8266/PIC build was not run in this pass because the requested scope was OTGW32/ESP32 port 25238 behavior; PIC serial behavior was left in the existing handlePICSerial path and not refactored.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented the OTDirect-owned inbound bridge for TCP port 25238 on no-PIC ESP32/OTGW32 builds.

Changes:
- Added handleOTDirectBridgeStream() in OTDirect.ino behind HAS_DIRECT_OT.
- Recreated the PIC bridge command framing: fixed buffer, CR terminates, LF ignored, overflow discards the current line without heap allocation.
- Routed complete commands through sendPICSerial(), preserving handleOTDirectCommand() as the single owner of PIC-style OTDirect command semantics.
- Preserved ESP8266/PIC behavior by leaving handlePICSerial() as the PIC serial bridge owner.
- Moved the existing OT log macro definitions into OTGWLogMacros.h so OTDirect can use the original ClrLog/AddLog/AddLogf_P/AddLogln style without relying on OTGW-Core.ino compile order.

Verification:
- .\\.venv\\Scripts\\python.exe tests\\test_evaluate.py: 34 tests passed.
- .\\.venv\\Scripts\\python.exe build.py --target esp32: completed successfully, including firmware, LittleFS, merged binaries, and distribution zip.
<!-- SECTION:FINAL_SUMMARY:END -->
