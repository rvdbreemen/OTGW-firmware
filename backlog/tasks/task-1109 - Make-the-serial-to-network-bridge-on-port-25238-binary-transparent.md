---
id: TASK-1109
title: Make the serial-to-network bridge on port 25238 binary transparent
status: Done
assignee:
  - '@claude'
created_date: '2026-09-02 20:53'
updated_date: '2026-09-02 21:23'
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
- [x] #1 Every byte read from OTGWSerial is forwarded to OTGWstream verbatim, without waiting for a line terminator
- [x] #2 CR, LF, NUL and bytes above 0x7F reach the network client unmodified; no CRLF is synthesised on the passthrough path
- [x] #3 A line that overflows MAX_BUFFER_READ still reaches the network client; only the OT parser drops it
- [x] #4 The passthrough is coalesced so a burst does not produce one TCP segment per byte, and it is flushed before handleOTGW() returns
- [x] #5 The serial drain is bounded by a byte cap as well as the existing line cap, so a payload without newlines cannot spin the loop
- [x] #6 Simulation replay lines still reach OTGWstream
- [x] #7 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Transport and parsing split in handleOTGW(): every byte is copied into a 64-byte stack buffer and written to OTGWstream as it is read, flushed when full and again before the function returns. dispatchOTGWInputLine() no longer touches OTGWstream; the simulation replay path writes its synthetic lines itself. Added HANDLE_OTGW_BYTES_PER_CALL 256 next to the existing line cap.

SimpleTelnet itself was checked and is already byte transparent: read()/write() do no IAC handling and no filtering (src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:200-263). The 0x80+ filter at :533 only applies to the line-mode input callback on the debug console, not to the raw read path OTGWstream uses.

build.py --firmware: Build completed successfully, build/OTGW-firmware-1.7.5-beta.6+1dc3307.ino.bin fresh. evaluate.py --quick: 37 checks, 35 passed, 0 failed, health 100%.

End-to-end device confirmation (a client seeing a prompt without a newline) is still pending a deploy.

DEVICE-VERIFIED on 192.168.88.68 running 1.7.5-beta.6+35dd50a (firmware + filesystem deployed OTA, LegacyPort25238Enabled turned on).

Raw capture of port 25238 for 22s: 98 recv() calls, 318 bytes, 69 of them NOT ending on a line terminator. Fragments such as R0 / 000 / PR / : S / =16. arrive as they come off the UART. Under the old line-buffered code a client could only ever receive whole CRLF-terminated lines, so this is a positive test that the passthrough is live.

CR/LF pairs arrive intact and unduplicated, confirming no synthesised terminator on the passthrough path.

Trade-off worth recording: the stream now costs more TCP segments than before (measured ~4.5 packets/s at idle OT traffic versus ~1.3/s line-buffered), because a flush happens per handleOTGW() tick rather than per line. At 9600 baud that is bounded and cheap; the 64-byte chunk only fills during a sustained burst.

Primary source for the reported symptom, found by the review fan-out: diagnose.asm:1544 declares the prompt as da "Enter test number: " - the 032 is an end-of-string sentinel that is never transmitted, so the prompt genuinely carries no newline and was stranded in sRead forever while the PIC blocked in GetString.
<!-- SECTION:NOTES:END -->
