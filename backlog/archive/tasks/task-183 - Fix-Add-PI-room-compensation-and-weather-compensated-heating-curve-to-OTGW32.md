---
id: TASK-183
title: 'Fix: Add PI room compensation and weather-compensated heating curve to OTGW32'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:32'
updated_date: '2026-04-08 23:08'
labels:
  - audit-fix
  - otgw32
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing implements a PI controller (loopPiCtrl) for room temperature compensation and a weather-compensated heating curve (getFlow: auto mode with exponent/gradient/offset). OTDirect.ino has no equivalent — it only passes CS=/TT= setpoints to the boiler. To reach feature parity, the OTGW32 master mode needs a configurable flow temperature calculator and optional PI room correction.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PI controller for room compensation (Kp, Ki, boost) is implemented in OTDirect or a companion module
- [x] #2 Weather-compensated flow temp (heating curve: exponent, gradient, offset, flowMax) is implemented
- [x] #3 Both features are configurable via REST API settings
- [x] #4 CH mode (ON/AUTO/OFF) selects between fixed flow, heating curve, and off
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add PI/heating-curve settings fields to OTDirectSettingsSection in OTGW-firmware.h
2. Add static state vars (integState, roomTempFilt, rspPrev, nextPiCtrl) in OTDirect.ino
3. Implement loopPiCtrl() and getFlowTemp() (heating curve / fixed / off) in OTDirect.ino
4. Call loopPiCtrl from loopOTDirect every 60s
5. Hook getFlowTemp into master mode scheduler for MsgID 1 when CH mode is AUTO/ON
6. Add settings serialization/deserialization in settingStuff.ino
7. Expose via REST in restAPI.ino OTD settings endpoint
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added PI room compensation and weather-compensated heating curve to OTGW32 OTDirect layer.

Changes:
- Added 11 new settings fields to OTDirectSettingsSection: iCHMode, fFlowTemp, fFlowMax, fRoomSetpoint, fGradient, fExponent, fOffset, bRoomCompEnabled, fKp, fKi, fKboost
- Settings serialized/deserialized in settingStuff.ino (OTDchmode, OTDflowtemp, etc.)
- Implemented getFlowTemp(): returns 0 (off), fixed flow, or heating curve computed from boiler MsgID 27 (outside temp)
- Heating curve follows OT-Thing getFlow() formula: flow = rsp + c1 * pow(rsp - outTmp, 1/exp) + offset
- Implemented loopPiCtrl() called every 60s: reads room temp (MsgID 24) and setpoint (MsgID 16) from boiler cache, computes P+I+boost correction, updates otPiDeltaT
- getFlowTemp() adds otPiDeltaT when bRoomCompEnabled, clips to [0, fFlowMax]
- In master mode, loopOTDirect() applies heating curve every 60s via enqueueWriteCommand(1, ...)
- REST API /api/v2/otdirect/settings GET/POST exposes all new fields
- resetTransientState() clears PI state on mode change or GW=R
<!-- SECTION:FINAL_SUMMARY:END -->
