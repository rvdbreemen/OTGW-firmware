---
id: TASK-1114
title: A disconnect race on port 25238 can write a fabricated 0xFF byte to the PIC
status: Done
assignee:
  - '@claude'
created_date: '2026-09-02 22:39'
updated_date: '2026-09-02 22:57'
labels: []
dependencies: []
ordinal: 272000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ser2net reader in OTGW-Core.ino does outByte = OTGWstream.read() into a uint8_t and enqueues it to the PIC unconditionally. AsyncSimpleTelnet::available() and ::read() are two separate critical sections that each take and release the lock (AsyncSimpleTelnet.h:411-421 and :424-431), so the AsyncTCP task can run _onClientDisconnect -> _releaseSlot -> _rx[idx].clear() in the gap between them. read() then returns -1, which truncates to 0xFF in the uint8_t and is written to the PIC UART as a byte no client ever sent.\n\nThis is byte fabrication, not the known interleaving or partial-write defects. Found by an adversarial audit of byte transparency on port 25238.\n\nThe correct shape already exists in this codebase: the sibling reader at OTDirect.ino:661-662 reads into an int and breaks on a negative value. The asymmetry between the two readers is the bug. The library is correct here: returning -1 on empty is the Stream contract, so this is a call-site fix.\n\n1.x is not affected: its reader is single-threaded against a polled WiFiClient, so nothing can mutate the stream between the two calls.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The ser2net reader reads into a signed int and stops the drain on a negative value instead of narrowing it to a byte
- [x] #2 No byte is enqueued to the PIC unless it was actually read from a client
- [x] #3 The two readers on port 25238, in OTGW-Core.ino and OTDirect.ino, handle an exhausted stream the same way
- [x] #4 build.bat is green for the default targets and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fixed in OTGW-Core.ino: the reader now takes an int from OTGWstream.read(), breaks the drain on a negative value, and only then narrows to the byte it enqueues. Mirrors OTDirect.ino:661-662, which already had the correct shape; the asymmetry between the two readers was the defect.

AC 3 verified by inspection of both readers after the change: both now read into a signed int and stop on a negative value.

Gates: build.bat green for esp32, esp32-classic and esp32-combo plus all three filesystem images; binaries fresh, stamped 2.0.0-alpha.361+87e5015. evaluate.py --quick 68/76 pass, 0 failed. The one grep hit for an error pattern in the build log is a string literal in the source ("Error: not implemented yet"), not a diagnostic.

Not reproduced live: the race needs a client disconnect landing between available() and read(), which is timing-dependent and cannot be staged reliably. The fix is unconditional and costs nothing when the stream is healthy, so it does not depend on reproducing the window.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stopped a disconnect race from writing a byte to the PIC that no client ever sent.

The ser2net reader did outByte = OTGWstream.read() into a uint8_t and enqueued it unconditionally. In AsyncSimpleTelnet, available() and read() are separate critical sections, so the AsyncTCP task can disconnect a client and clear its RX ring in the gap. read() then returns -1, which narrows to 0xFF and reaches the PIC UART as a fabricated command byte.

The reader now takes an int, breaks the drain on a negative value, and narrows only a byte it actually read. The correct shape already existed at OTDirect.ino:661-662; this removes the asymmetry between the two readers on the same port.

Distinct from the outbound re-framing fixed by TASK-1111 and survived it, because it sits on the other half of the loop. 1.x is unaffected: its reader is single-threaded against a polled WiFiClient, so the window does not exist there.

Found by an adversarial audit of byte-exactness on port 25238 that read both transports and both library pins.
<!-- SECTION:FINAL_SUMMARY:END -->
