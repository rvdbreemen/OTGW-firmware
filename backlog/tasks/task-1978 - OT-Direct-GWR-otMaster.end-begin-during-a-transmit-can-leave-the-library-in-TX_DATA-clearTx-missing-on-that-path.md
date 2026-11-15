---
id: TASK-1978
title: >-
  OT-Direct GW=R: otMaster.end()/begin() during a transmit can leave the library
  in TX_DATA (clearTx() missing on that path)
status: To Do
assignee: []
created_date: '2026-11-15T15:35:00Z'
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
- [ ] #1 Host test with a fake library in TX_DATA + GW=R: red on the current code (line stays TX_DATA past the GW=R), green on the fix (IDLE right after the GW=R, no stall restart counted)
- [ ] #2 Bench smoke on an OT-Direct unit by the bench tester: GW=R while frames run, frames continue, otdlinetxrestarts unchanged
<!-- AC:END -->
