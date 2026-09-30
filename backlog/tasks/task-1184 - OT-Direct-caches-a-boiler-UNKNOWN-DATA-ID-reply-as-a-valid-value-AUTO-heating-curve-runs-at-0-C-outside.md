---
id: TASK-1184
title: >-
  OT-Direct caches a boiler UNKNOWN-DATA-ID reply as a valid value (AUTO heating
  curve runs at 0 C outside)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 11:41'
updated_date: '2026-09-30 12:31'
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
- [x] #1 An UNKNOWN-DATA-ID reply never marks a cache slot valid; it clears the slot's valid flag, so a Data-ID the boiler stops supporting is no longer served from an old value
- [x] #2 In master mode a thermostat READ of a Data-ID the boiler answered with UNKNOWN-DATA-ID gets UNKNOWN-DATA-ID (type 7), not READ-ACK(0)
- [x] #3 In AUTO heating-curve mode, a boiler that answers MsgID 27 with UNKNOWN-DATA-ID makes getFlowTemp() use the fixed-flow fallback, not a 0.0 C outside temperature
- [x] #4 Old-vs-fix host proof that compiles the REAL handleMasterResponse and getFlowTemp code (sliced, not re-implemented): OLD shows slot 27 valid after an UNKNOWN reply and a curve flow computed for 0.0 C; FIX shows neither
- [x] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation 2026-09-30 (alpha.391):
- otBoilerCacheStore() (the TASK-1177 helper) clears the id's valid flag on an UNKNOWN-DATA-ID reply instead of storing its data bytes; READ-ACK and WRITE-ACK are stored as before.
- The flame-ratio check also requires a valid MsgID 0 slot, so an UNKNOWN reply to MsgID 0 no longer feeds stale or zero status bits to flameRatioSet().
Host proof, build_and_run_override_reply.ps1 -Suite 1184 -OldVsFix -OldRev HEAD (OLD = fea1f537a). The harness now also slices the PI state and getFlowTemp() (the stub is gone) and the settings double carries the heating-curve fields. FIX 6/6; OLD 2/6, failing exactly U1-U4:
- U1 master mode, AUTO curve, READ(27) answered UNKNOWN: OLD cache 1B:0000, getFlowTemp() 50.00 (curve at 0.0 C outside); FIX cache -, 45.00 (fixed-flow fallback).
- U2 master mode thermostat READ(27) after that: OLD replies C01B0000 (READ-ACK 0); FIX F01B0000 (UNKNOWN-DATA-ID).
- U3 READ-ACK(27, 5.0) then UNKNOWN: OLD keeps a valid 1B:0000 and 50.00; FIX clears the slot, 45.00.
- U4 READ(0) answered UNKNOWN: OLD cache 00:0000 and flameRatioSet(false); FIX neither.
- Controls U5 (READ-ACK(27, 5.0) drives the curve to 42.50) and U6 (WRITE-ACK(1) cached) byte-identical.
- Regression: the TASK-1177 suite 7/7 byte-identical; the TASK-1178 suite 40/40, where C2, C7b, C7e and C8b (boiler UNKNOWN replies) now also assert 'nothing cached' and are the only cases OLD fails. C8b was not on my predicted list; the byte-identical check caught it.
- The TASK-1178 verdict re-run with the tightened harness (-OldRev 9c1823528) is still PASS: FIX 40/40, OLD 13/40.
- Slice audit: 48 OTDirect.ino slices (24 per side, incl. PI state/getFlowTemp) byte-equal to their source.
Transcripts: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1184-run-oldvsfix-fea1f537a.txt and TASK-1178-rerun-after-1184-harness.txt.

AC#5 evidence: build.bat --target all at alpha.391: esp32, esp32-classic, esp32-combo firmware and filesystem all SUCCESS, 'Build completed successfully!'; fresh binaries (.pio/build/*/firmware.bin 14:23:24 / 14:26:27 / 14:29:40), flash 79.4% / 77.1% / 81.3%. Log: %LOCALAPPDATA%/OTGW-capture/build-alpha391-task1184.log. evaluate.py --quick: 70 passed, 0 warnings, 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct no longer caches a boiler UNKNOWN-DATA-ID reply as a valid value. The OpenTherm library reports UNKNOWN-DATA-ID as SUCCESS, so handleMasterResponse() stored the reply's data bytes (0x0000) and marked the slot valid. With a boiler that has no outside sensor, the AUTO heating curve then ran at 0.0 C outside (flow 50.0 instead of the fixed 45.0 with the default settings), and master mode answered a thermostat READ of an unsupported id with READ-ACK(0).
- otBoilerCacheStore() clears the slot on UNKNOWN-DATA-ID; the flame-ratio check requires a valid MsgID 0 slot.
Evidence per AC:
- AC#1: U3, a valid 5.0 C followed by UNKNOWN clears slot 27 (OLD keeps a valid 1B:0000).
- AC#2: U2, master-mode READ(27) answered F01B0000 UNKNOWN-DATA-ID (OLD C01B0000 READ-ACK 0).
- AC#3: U1, AUTO curve falls back to the fixed 45.00 (OLD 50.00, the curve at 0.0 C).
- AC#4: host harness -Suite 1184 compiling the real sliced handleMasterResponse and getFlowTemp: FIX 6/6, OLD fails exactly U1-U4; controls byte-identical; the TASK-1177 suite 7/7 and the TASK-1178 suite 40/40 as regressions (C2/C7b/C7e/C8b tightened to 'nothing cached').
- AC#5: alpha.391, three targets built fresh, evaluate --quick 0 FAIL.
No hardware run: the OTGW32 bench has no boiler attached.
<!-- SECTION:FINAL_SUMMARY:END -->
