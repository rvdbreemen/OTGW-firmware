---
id: TASK-1114
title: A disconnect race on port 25238 can write a fabricated 0xFF byte to the PIC
status: To Do
assignee: []
created_date: '2026-09-02 22:39'
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
- [ ] #1 The ser2net reader reads into a signed int and stops the drain on a negative value instead of narrowing it to a byte
- [ ] #2 No byte is enqueued to the PIC unless it was actually read from a client
- [ ] #3 The two readers on port 25238, in OTGW-Core.ino and OTDirect.ino, handle an exhausted stream the same way
- [ ] #4 build.bat is green for the default targets and evaluate.py --quick shows no new failures
<!-- AC:END -->
