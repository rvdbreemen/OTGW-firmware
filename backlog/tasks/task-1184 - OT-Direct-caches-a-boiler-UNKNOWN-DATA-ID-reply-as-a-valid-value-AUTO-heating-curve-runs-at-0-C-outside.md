---
id: TASK-1184
title: >-
  OT-Direct caches a boiler UNKNOWN-DATA-ID reply as a valid value (AUTO heating
  curve runs at 0 C outside)
status: To Do
assignee: []
created_date: '2026-09-30 11:41'
labels:
  - bug
  - otdirect
dependencies: []
priority: high
ordinal: 307000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0, OT-Direct only, predates this session)
handleMasterResponse() stores every SUCCESS reply in otBoilerCache and marks the slot valid. The OpenTherm library reports a boiler UNKNOWN-DATA-ID reply as SUCCESS: isValidResponse() accepts READ_ACK, WRITE_ACK and UNKNOWN_DATA_ID (src/libraries/OpenTherm/src/OpenTherm.cpp:518-526, status set at :444). So a boiler that does not support a Data-ID leaves a VALID cache slot holding the reply's data bytes (normally 0x0000).
Evidence, three sources: the library code above; the cache write in handleMasterResponse's SUCCESS branch (OTDirect.ino, before the 3-strike UNKNOWN check); the TASK-1178 host harness, which runs the real handleMasterResponse and dumps 'cache 01:0000' after a boiler UNKNOWN in case C2 (OLD and FIX alike).
Consequences:
- Master mode, AUTO heating curve (settings.otd.iCHMode == 2): getFlowTemp() uses otBoilerCache[27] when otBoilerCacheValid[27] is set. A boiler without an outside sensor answers MsgID 27 with UNKNOWN-DATA-ID, so the curve runs at 0.0 C outside instead of falling back to the fixed flow temperature (settings.otd.fFlowTemp).
- Master mode: handleMasterModeSlaveFrame() answers a thermostat READ of such an ID with READ-ACK(0x0000) instead of UNKNOWN-DATA-ID.
- Other cache readers (the room-temperature PI path on 24/16, the flame/CH status on 0, flow on 25, otIsVentSlave/otDirectBoilerPresent on 3) take the UNKNOWN data as a boiler value.
Found by the TASK-1178 fixup agent. Same helper as TASK-1177 (the cache store); do after TASK-1177 so each fix ships under its own prerelease tag.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An UNKNOWN-DATA-ID reply never marks a cache slot valid; it clears the slot's valid flag, so a Data-ID the boiler stops supporting is no longer served from an old value
- [ ] #2 In master mode a thermostat READ of a Data-ID the boiler answered with UNKNOWN-DATA-ID gets UNKNOWN-DATA-ID (type 7), not READ-ACK(0)
- [ ] #3 In AUTO heating-curve mode, a boiler that answers MsgID 27 with UNKNOWN-DATA-ID makes getFlowTemp() use the fixed-flow fallback, not a 0.0 C outside temperature
- [ ] #4 Old-vs-fix host proof that compiles the REAL handleMasterResponse and getFlowTemp code (sliced, not re-implemented): OLD shows slot 27 valid after an UNKNOWN reply and a curve flow computed for 0.0 C; FIX shows neither
- [ ] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->
