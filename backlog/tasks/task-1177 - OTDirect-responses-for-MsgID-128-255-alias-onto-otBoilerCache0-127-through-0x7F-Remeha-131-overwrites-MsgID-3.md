---
id: TASK-1177
title: >-
  OTDirect: responses for MsgID 128-255 alias onto otBoilerCache[0-127] through
  '& 0x7F' (Remeha 131 overwrites MsgID 3)
status: To Do
assignee: []
created_date: '2026-09-30 09:10'
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
- [ ] #1 Every otBoilerCache / otBoilerCacheValid index in OTDirect.ino is either < 128 by construction or guarded so MsgIDs 128-255 are never stored in, read from, or reported as another id's slot; the four sites (:1237-1239, :1292-1294, :1306-1310, :2523-2524) are covered
- [ ] #2 In master mode a READ of MsgID 128-255 is answered with UNKNOWN-DATA-ID (type 7) rather than with another id's cached value
- [ ] #3 Old-vs-fix host proof that compiles the REAL cache-update code (sliced by anchor, not re-implemented): on OLD a MsgID 131 READ_ACK changes otBoilerCache[3] and sets otBoilerCacheValid[3]; on the FIX slot 3 is untouched and a MsgID 128 reply does not drive flameRatioSet
- [ ] #4 otDirectBoilerPresent() and otIsVentSlave() are shown to depend only on genuine MsgID 3 replies after the fix
- [ ] #5 The change ships in one commit with its own prerelease bump; build.bat (esp32 and esp32-combo) SUCCESS with fresh binaries; evaluate.py --quick shows no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from follow-up (a) in the TASK-1173 fix plan, after re-reading the four sites in OTDirect.ino. Follow-up (b) (gateway mode may never answer an override-modified thermostat frame) is unverified and is NOT part of this task.
<!-- SECTION:NOTES:END -->
