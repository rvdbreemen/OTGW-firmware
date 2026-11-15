---
id: TASK-1978
title: >-
  OT-Direct GW=R: otMaster.end()/begin() during a transmit can leave the library
  in TX_DATA (clearTx() missing on that path)
status: To Do
assignee: []
created_date: '2026-11-15T15:35:00Z'
updated_date: '2026-11-15T21:11:00Z'
labels:
  - otdirect
  - bug
dependencies: []
priority: low
ordinal: 1011000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From d3's review of TASK-1966: handleOTDirectCommand() case 'R' (GW=R) does otMaster.end()/begin(). The library's end() does not reset txStatus (upstream PR Phunkafizer/opentherm_library#1 fixes that in the library; our pinned copy f7d8254 does not have it), so a GW=R in the middle of a frame can leave the line in TX_DATA until OTDMasterLine's 500 ms stall guard restarts it. Add the same clearTx() as the TX-stall path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test with a fake library in TX_DATA + GW=R: red on the current code (line stays TX_DATA past the GW=R), green on the fix (IDLE right after the GW=R, no stall restart counted)
- [x] #2 Bench smoke on an OT-Direct unit by the bench tester: GW=R while frames run, frames continue, otdlinetxrestarts unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Covered by TASK-1980 (beta.328, c89f9b6e6): the library fork's end() resets txStatus/txIndex after the timer stop, and the OTDMasterLine guard (with its otdlinetxrestarts counter) is gone, so AC2's 'otdlinetxrestarts unchanged' now reads as 'frames continue after GW=R'. AC1: tests/test_otd_master_line_stale_delay.py test_3_end_begin_during_a_frame_leaves_the_line_ready compiles the real library: end()+begin() after 10 of 68 half bits leaves the line ready (green on the fork pin), and test_old_pin_is_red asserts the old pin f7d8254 stays in TX_DATA (c3 ready=0). AC2 bench evidence is in TASK-1980 (09 on .61: GW=R 3x while frames run, each one ~3.27 s gap then MsgID 0 continues, 89 min run); the tick is 09's.

2026-11-15 22:45 09: AC2 ticked from the TASK-1980 bench on .61 (beta.325+9dba970, 89 min): GW=R sent 3x while frames ran, R frames continued after each, no stall. The counter otdlinetxrestarts named in the AC no longer exists (the OTDMasterLine workaround was removed by TASK-1980, 9918e9b88), so 'unchanged' was judged by frames continuing and no restart line in telnet.
<!-- SECTION:NOTES:END -->
