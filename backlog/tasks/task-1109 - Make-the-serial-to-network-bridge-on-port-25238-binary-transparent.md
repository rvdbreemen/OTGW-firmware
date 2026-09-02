---
id: TASK-1109
title: Make the serial-to-network bridge on port 25238 binary transparent
status: To Do
assignee: []
created_date: '2026-09-02 20:53'
labels: []
dependencies: []
ordinal: 204000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
handleOTGW() assembles serial bytes into sRead and only forwards them to OTGWstream when a CR or LF arrives (OTGW-Core.ino:4666 via dispatchOTGWInputLine at 3162). Two consequences reported by Schelte Bron: output from the diagnose firmware only reaches a TCP client once a full line with newline exists, so a prompt such as 'Enter test number:' never shows up; and any 0x0D/0x0A inside a payload is consumed as a terminator and replaced by a synthesised CRLF, while an embedded 0x00 truncates the forwarded string. Transport and parsing are conflated in one loop. Forward every received byte verbatim to the network as it arrives, and keep the line assembly purely for processOT(). Coalesce the passthrough in a small stack buffer because a per-byte WiFiClient::write() emits one TCP segment per byte on ESP8266.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every byte read from OTGWSerial is forwarded to OTGWstream verbatim, without waiting for a line terminator
- [ ] #2 CR, LF, NUL and bytes above 0x7F reach the network client unmodified; no CRLF is synthesised on the passthrough path
- [ ] #3 A line that overflows MAX_BUFFER_READ still reaches the network client; only the OT parser drops it
- [ ] #4 The passthrough is coalesced so a burst does not produce one TCP segment per byte, and it is flushed before handleOTGW() returns
- [ ] #5 The serial drain is bounded by a byte cap as well as the existing line cap, so a payload without newlines cannot spin the loop
- [ ] #6 Simulation replay lines still reach OTGWstream
- [ ] #7 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
