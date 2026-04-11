---
id: TASK-247.2
title: Add OTDirect conditional debug macros and fill debug gaps in OTDirect.ino
status: Done
assignee:
  - '@claude'
created_date: '2026-04-11 09:12'
updated_date: '2026-04-11 09:27'
labels:
  - otdirect
  - esp32
  - debug
dependencies: []
parent_task_id: TASK-247
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Define OTDDebugTf/OTDDebugTln/OTDDebugf macros gated by state.debug.bOTDirect (default true, same pattern as OTDebug*). Replace all raw DebugTf/DebugTln calls with the new macros. Fill missing debug in: PI controller loop (loopPiCtrl), flame ratio accumulation/commit, frame override table resolution, schedule iteration, command queue processing, handleMasterResponse hot path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTDDebug* macros defined in OTDirect.ino, identical pattern to OTDebug*
- [x] #2 All existing DebugTf/DebugTln in OTDirect.ino replaced with OTDDebug* equivalents
- [x] #3 loopPiCtrl logs: error, P-term, I-term, output, clamping
- [ ] #4 flameRatioAccum and flameRatioBufCommit log duty/frequency accumulation values
- [ ] #5 handleMasterResponse logs frame type and override decisions
- [ ] #6 handleOTDirectCommand logs command received and action taken
- [ ] #7 Build clean
<!-- AC:END -->
