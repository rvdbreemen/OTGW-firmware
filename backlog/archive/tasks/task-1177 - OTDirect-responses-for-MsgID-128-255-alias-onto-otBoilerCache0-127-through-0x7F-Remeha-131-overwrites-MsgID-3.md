---
id: TASK-1177
title: >-
  OTDirect: responses for MsgID 128-255 alias onto otBoilerCache[0-127] through
  '& 0x7F' (Remeha 131 overwrites MsgID 3)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 09:10'
updated_date: '2026-09-30 12:10'
labels:
  - bug
  - otdirect
dependencies: []
priority: medium
ordinal: 300000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0, OT-Direct only)
otBoilerCache[128] / otBoilerCacheValid[128] (OTDirect.ino:268-269) are indexed with '(response >> 16) & 0x7F' / 'msgId & 0x7F'. A response for MsgID 128+n therefore lands in the slot of MsgID n. This stays in bounds, so it produces wrong data, not memory corruption.

Sites (verified 2026-09-30 by reading the code; found by the TASK-1173 judge):
- OTDirect.ino:1237-1239 loopback cache write, :1292-1294 gateway/master cache write: cacheId = (response >> 16) & 0x7F; otBoilerCache[cacheId] = ...; otBoilerCacheValid[cacheId] = true.
- :1306-1310 flame bit: cacheId0 = (response >> 16) & 0x7F; if (cacheId0 == 0) flameRatioSet(...), so a MsgID 128 reply drives the flame ratio with 128's data.
- :2523-2524 master-mode slave handler: READ of MsgID 128+n is answered with otBoilerCache[n].

Consequences:
- A Remeha MsgID 131 READ_ACK (dF/dU codes, forwarded in gateway mode) overwrites cached MsgID 3 (Slave Config / MemberID) and marks it valid. otDirectBoilerPresent() (:296-298) then reports a boiler even if the boiler never answered MsgID 3, and otIsVentSlave() (:279-281) reads its config byte from the dF/dU data, which steers the MsgID 70/71 scheduling at :1451.
- In master mode, a thermostat READ of MsgID 131 gets MsgID 3's data back.

Trigger: an OEM data-id >= 128 on the bus in OT-Direct gateway or master mode (Remeha 131-133 appear in the coverage fixture). The PIC path is not affected.
1.x: not applicable (no OT-Direct).
Related: TASK-1173 (same '>= 128' class, out-of-bounds counters), TASK-1072 (loopback range).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every otBoilerCache / otBoilerCacheValid index in OTDirect.ino is either < 128 by construction or guarded so MsgIDs 128-255 are never stored in, read from, or reported as another id's slot; the four sites (:1237-1239, :1292-1294, :1306-1310, :2523-2524) are covered
- [x] #2 In master mode a READ of MsgID 128-255 is answered with UNKNOWN-DATA-ID (type 7) rather than with another id's cached value
- [x] #3 Old-vs-fix host proof that compiles the REAL cache-update code (sliced by anchor, not re-implemented): on OLD a MsgID 131 READ_ACK changes otBoilerCache[3] and sets otBoilerCacheValid[3]; on the FIX slot 3 is untouched and a MsgID 128 reply does not drive flameRatioSet
- [x] #4 otDirectBoilerPresent() and otIsVentSlave() are shown to depend only on genuine MsgID 3 replies after the fix
- [x] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. One guarded store helper for the two cache writes (loopback in sendMasterRequestAsync, gateway/master in handleMasterResponse): take the full 8-bit Data-ID and store only ids < 128, the range the cache covers. No 256-slot cache: nothing polls OEM ids, and AC#2 wants UNKNOWN-DATA-ID for 128-255 anyway (KISS).
2. Flame ratio: compare the full 8-bit Data-ID with 0, so a MsgID 128 reply no longer drives flameRatioSet().
3. Master-mode slave handler: answer READ of 128-255 with UNKNOWN-DATA-ID (type 7); never index the cache with msgId & 0x7F.
4. Also fix the fifth '& 0x7F' site: the satSimulationBlocksBusTx() trace string (OTDirect.ino ~1264) shows MID=3 for MsgID 131 on the SAT dashboard. Display-only, same class, one character.
5. Host proof: extend the TASK-1178 override-reply harness (it already slices the real sendMasterRequestAsync and handleMasterResponse against a FakeOpenTherm) with 1177 cases, and slice handleMasterModeSlaveFrame, otIsVentSlave and otDirectBoilerPresent too. OLD = the TASK-1178 commit, FIX = working tree; OLD must show slot 3 overwritten by a 131 READ_ACK, the flame driven by 128, and 131 answered with 3's data; FIX must show none of that.
6. Sequencing: starts after TASK-1178 is committed (same functions). Then bump, build.bat esp32 + esp32-combo (+ esp32-classic), evaluate.py --quick, one commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from follow-up (a) in the TASK-1173 fix plan, after re-reading the four sites in OTDirect.ino. Follow-up (b) (gateway mode may never answer an override-modified thermostat frame) is unverified and is NOT part of this task.

Implementation 2026-09-30 (alpha.390):
- One store helper, otBoilerCacheStore(response): takes the full 8-bit Data-ID and stores only ids < sizeof(otBoilerCacheValid) (128). Used by the loopback cache write in sendMasterRequestAsync and the SUCCESS cache write in handleMasterResponse. Placed right before sendMasterRequestAsync, inside the range the host harness already slices.
- Flame ratio: compares the full 8-bit Data-ID with 0.
- Master-mode slave handler: msgId < sizeof(otBoilerCacheValid) && otBoilerCacheValid[msgId], else UNKNOWN-DATA-ID.
- Fifth site: the satSimulationBlocksBusTx() trace string now uses & 0xFF (it showed MID=3 for MsgID 131 on the SAT dashboard; display only).
- No '& 0x7F' remains in OTDirect.ino (grep count 0).
Host proof: the TASK-1178 harness gained a second suite (-Suite 1177, cases K1-K7) plus real slices of otIsVentSlave/otDirectBoilerPresent and handleMasterModeSlaveFrame (the stub is gone) and a recording flameRatioSet(). Run: build_and_run_override_reply.ps1 -Suite 1177 -OldVsFix -OldRev HEAD (OLD = f5c9e5d22). FIX 7/7; OLD 3/7, failing exactly K1-K4:
- K1 gateway READ-ACK(131, C012): OLD cache=03:C012 present=1 vent=1; FIX cache=- present=0 vent=0.
- K2 gateway READ-ACK(128, 0008): OLD cache=00:0008 flame=1; FIX cache=- flame=-.
- K3 loopback READ(131) -> UNKNOWN(131): OLD cache=03:0000; FIX cache=-.
- K4 master mode READ(131) with MsgID 3 cached: OLD replies C0831234 (READ-ACK with 3's data); FIX 70830000 (UNKNOWN-DATA-ID).
- Controls K5-K7 (genuine MsgID 3 and MsgID 0 replies, master READ(3)) byte-identical on OLD and FIX; all 40 TASK-1178 cases run as a regression on both sides, byte-identical (40/40).
- The TASK-1178 verdict re-run with the extended harness (-OldRev 9c1823528) is unchanged: FIX 40/40, OLD 13/40, 25/13/2, RESULT PASS.
- Slice audit: all 46 OTDirect.ino slices (23 per side, incl. the new ones) byte-equal to their source.
Transcripts: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1177-run-oldvsfix-f5c9e5d22.txt and TASK-1178-rerun-after-1177-harness.txt.

AC#5 evidence: build.bat --target all at alpha.390: esp32, esp32-classic, esp32-combo firmware and filesystem all SUCCESS, 'Build completed successfully!'; fresh binaries (.pio/build/*/firmware.bin 14:03:08 / 14:06:11 / 14:09:16), flash 79.4% / 77.1% / 81.3%. Log: %LOCALAPPDATA%/OTGW-capture/build-alpha390-task1177.log. evaluate.py --quick: 70 passed, 0 warnings, 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct no longer aliases replies for OEM data-ids 128-255 onto otBoilerCache[0-127]. Masking the Data-ID with 0x7F put a MsgID 128+n reply in MsgID n's slot: a Remeha MsgID 131 reply overwrote MsgID 3 (Slave Config), so otDirectBoilerPresent() could report a boiler that never answered MsgID 3, otIsVentSlave() read dF/dU data as the slave configuration, a MsgID 128 reply drove the flame ratio, and master mode answered a READ of 131 with MsgID 3's data.
- One store helper, otBoilerCacheStore(), stores data-ids 0-127 only; the loopback and gateway cache writes use it.
- The flame-ratio check and the master-mode READ path use the full 8-bit Data-ID; a READ of 128-255 in master mode is answered UNKNOWN-DATA-ID.
- The SAT simulation trace shows the real MsgID (& 0xFF).
Evidence per AC:
- AC#1: grep finds no '& 0x7F' left in OTDirect.ino; all four sites plus the trace changed.
- AC#2 and AC#3: host harness (-Suite 1177, real sliced code incl. handleMasterModeSlaveFrame): FIX 7/7, OLD 3/7 failing exactly K1-K4 (OLD: cache 03:C012 after a 131 reply, flame=1 after a 128 reply, loopback 131 in slot 3, master READ(131) answered C0831234; FIX: none of these, READ(131) answered 70830000).
- AC#4: K1 shows otDirectBoilerPresent()/otIsVentSlave() stay 0 after a 131 reply on FIX (1/1 on OLD); control K5 shows a genuine MsgID 3 reply still drives them on both.
- AC#5: alpha.390, three targets built fresh, evaluate --quick 0 FAIL.
Regression: all 40 TASK-1178 cases byte-identical OLD vs FIX; the TASK-1178 verdict re-run with the extended harness is unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
