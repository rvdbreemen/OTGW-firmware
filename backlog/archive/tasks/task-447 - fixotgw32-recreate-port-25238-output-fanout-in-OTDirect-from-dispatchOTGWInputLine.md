---
id: TASK-447
title: >-
  fixotgw32-recreate-port-25238-output-fanout-in-OTDirect-from-dispatchOTGWInputLine
status: Done
assignee:
  - '@codex'
created_date: '2026-04-27 10:28'
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
  - TASK-446
references:
  - 'src/OTGW-firmware/OTGW-Core.ino:3078'
  - 'src/OTGW-firmware/OTDirect.ino:447'
  - 'src/OTGW-firmware/OTDirect.ino:1674'
  - 'src/OTGW-firmware/OTDirect.ino:2007'
documentation:
  - 'docs/manuals/en/ch09-api-reference.md:1138'
  - 'docs/c4/c4-container.md:164'
  - 'docs/manuals/en/ch06-network.md:198'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support
Coding agent tag: codex
Depends on: TASK-445, TASK-446
User design direction: when no PIC is available, OTDirect should contain the actual bridge handler for TCP port 25238. Recreate the OTDirect bridge behavior from the existing OTGW/PIC handling functions rather than inventing a separate format.

Problem found during the port 25238 audit: ESP32/OTGW32 does not currently have a clear outbound path that writes OTDirect traffic and synthesized PIC-style command responses to `OTGWstream` clients on TCP port 25238.

Evidence:
- `src/OTGW-firmware/OTGW-Core.ino:3078-3086` has `dispatchOTGWInputLine()`, which writes a PIC-origin line to `OTGWstream` with CRLF and then calls `processOT()`. This is the existing bridge fanout model for serial-origin lines.
- `src/OTGW-firmware/OTGW-Core.ino:4339-4342` calls `dispatchOTGWInputLine(sRead, bytes_read)` for complete PIC serial lines.
- `src/OTGW-firmware/OTDirect.ino:447-458` formats OTDirect frames as `T/B/R/A%08lX` strings and feeds them into `processOT()` through `bridgeFrameToParser()`.
- `src/OTGW-firmware/OTDirect.ino:1674-1687` formats synthesized PIC-style command responses such as `XX: value` and feeds them into `processOT()` through `synthesizeResponse()`.
- `processOT()` handles MQTT/WebSocket/state side effects, but the current OTDirect paths do not obviously write those same lines to `OTGWstream` clients.
- `docs/manuals/en/ch09-api-reference.md:1138-1159` documents port 25238 as the raw OTGW serial stream for tools such as OTmonitor.

Why this matters:
A TCP serial bridge is bidirectional. Fixing only inbound commands means OTmonitor or a raw TCP client can send commands but may not see OTDirect raw frames or the `XX: value` / error responses that a PIC-backed gateway would produce. For PIC builds, `dispatchOTGWInputLine()` already fans complete serial-origin lines out to the 25238 stream and then hands them to `processOT()`. OTGW32 needs the same fanout shape in an OTDirect-owned bridge function.

Implementation guidance:
Keep OTDirect as the owner for no-PIC bridge behavior. Add a small OTDirect-side bridge write helper, for example `otDirectBridgeWriteLine(const char* line, size_t len)`, guarded by `#if HAS_DIRECT_OT`. Model the helper on `dispatchOTGWInputLine()`:
- write the line bytes to `OTGWstream`,
- write CR,
- write LF,
- do not allocate heap,
- then allow the existing `processOT()` call path to remain responsible for decoding, MQTT, WebSocket, and state side effects.

Places to consider:
- In `bridgeFrameToParser()`, after formatting the raw frame line, fan it out to `OTGWstream` using the OTDirect bridge helper and then call `processOT(buf, 9, otHideReports)` as today.
- In `synthesizeResponse()`, after formatting the `XX: value` line, fan it out to `OTGWstream` using the OTDirect bridge helper and then call `processOT(buf, strlen(buf))` as today.
- For short error responses emitted as `processOT("NG", 2)`, `processOT("BV", 2)`, `processOT("OR", 2)`, etc., decide whether the bridge should also emit those two-letter responses. If yes, centralize this in the OTDirect bridge helper or a tiny response wrapper rather than adding scattered writes at every error site.

Important constraints:
- PIC builds must keep using the existing PIC serial fanout path and must not double-write each PIC line to port 25238.
- The OTDirect bridge output helper must live in `OTDirect.ino` behind `#if HAS_DIRECT_OT`.
- The bridge output helper must not allocate heap and must not use Arduino `String`.
- Use CRLF line endings for compatibility with OTmonitor-style clients.
- Keep raw OpenTherm frame formatting identical to current OTDirect parser input: prefix char plus eight uppercase hex digits.

Do not call PlatformIO directly for verification; use `build.py`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 On `HAS_DIRECT_OT` builds, raw OTDirect frame lines formatted as prefix plus eight hex digits are written to connected `OTGWstream` clients on port 25238.
- [x] #2 On `HAS_DIRECT_OT` builds, synthesized PIC-style command responses from `synthesizeResponse()` are written to connected `OTGWstream` clients on port 25238 with CRLF line endings.
- [x] #3 The no-PIC outbound bridge helper lives in `OTDirect.ino` behind `#if HAS_DIRECT_OT`; PIC serial fanout remains owned by the existing PIC path in `OTGW-Core.ino`.
- [x] #4 No line is processed twice by `processOT()`, and PIC builds do not double-write the same serial-origin line to port 25238.
- [x] #5 The implementation uses the existing global `OTGWstream` object and fixed buffers only; it does not add `String`, heap allocation, or another TCP server.
- [x] #6 Manual or automated verification proves a TCP client connected to port 25238 on an ESP32/OTGW32 build can see at least one OTDirect raw frame or synthesized command response after sending a supported command.
- [x] #7 Verification is performed with `build.py --target esp32`; document any post-build artifact-lock failure separately from compile/link success.
- [x] #8 The OTDirect output helper is explicitly modeled on `dispatchOTGWInputLine()` (`OTGW-Core.ino:3078-3086`): write line bytes to `OTGWstream`, then CR, then LF, without changing `processOT()` ownership.
- [x] #9 OTDirect bridge output is implemented in `OTDirect.ino` behind `#if HAS_DIRECT_OT`; PIC bridge output remains in the existing OTGW/PIC path.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added OTDirect-owned fanout helper that writes bridge output to OTGWstream with CRLF. bridgeFrameToParser() now fans out raw OTDirect frames before processOT(), and synthesizeResponse() does the same for synthesized PIC-style command responses.

Verification: automated regression coverage in tests/test_evaluate.py checks that OTDirect raw frame and synthesized response paths write to OTGWstream. No physical TCP-client hardware smoke was run in this pass. build.py --target esp32 completed successfully after clearing stale build processes from the earlier artifact-lock failure.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented the OTDirect outbound fanout for TCP port 25238.

Changes:
- Added otDirectBridgeWriteLine() in OTDirect.ino to mirror dispatchOTGWInputLine(): payload bytes, CR, LF, no heap allocation.
- Updated bridgeFrameToParser() so raw OTDirect frame lines are written to OTGWstream before processOT().
- Updated synthesizeResponse() so PIC-style command responses are written to OTGWstream before processOT().
- Kept PIC serial output fanout owned by the existing OTGW-Core.ino PIC path, avoiding double-writes on PIC builds.

Verification:
- .\\.venv\\Scripts\\python.exe tests\\test_evaluate.py: 34 tests passed, including the OTDirect 25238 outbound fanout regression checks.
- .\\.venv\\Scripts\\python.exe build.py --target esp32: completed successfully, including firmware, LittleFS, merged binaries, and distribution zip.
<!-- SECTION:FINAL_SUMMARY:END -->
