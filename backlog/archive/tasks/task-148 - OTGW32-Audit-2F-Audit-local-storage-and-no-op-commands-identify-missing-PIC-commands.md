---
id: TASK-148
title: >-
  OTGW32-Audit-2F: Audit local storage and no-op commands; identify missing PIC
  commands
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:18'
updated_date: '2026-04-08 22:35'
labels:
  - audit
  - otgw32
  - phase-2
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify local storage commands (SB, IT, OH, RS, MI, FS, PS) and no-op commands (GA, GB, LA-LF, VR, TS, FT, DP) against PIC spec. Also cross-reference the complete otgw-6.6 PIC command set against handleOTDirectCommand() to identify any commands present in the PIC firmware but not implemented in OTDirect. Known candidates to check: TW= (thermostat wiring), AX=/DX= (add/delete extra on thermostat side).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SB= constrains setback temp to 1-30C and persists to flash
- [x] #2 IT= stores ignore-transitions flag (0/1)
- [x] #3 OH= stores override-high-byte flag (0/1)
- [x] #4 RS= maps all 8 counter codes (HBS/HBH/HPS/HPH/WBS/WBH/WPS/WPH) to correct MsgIDs (116-123)
- [x] #5 MI= validates 100-2550ms range, stores in centiseconds for PR=N response
- [x] #6 FS= persists to flash
- [x] #7 PS=1 suppresses individual frame output and emits summary per cycle
- [x] #8 GA=/GB= store GPIO function chars for PR=G response
- [x] #9 LA=-LF= store LED function chars for PR=L response
- [x] #10 VR= stores voltage reference digit for PR=V response
- [x] #11 TS= stores temp sensor function char for PR=D response
- [x] #12 FT= stores force-thermostat char
- [x] #13 DP= acknowledged as no-op
- [x] #14 Complete PIC command list from otgw-6.6 is cross-checked; missing commands result in audit-fix tasks
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit of local storage, no-op commands, and missing PIC commands complete.

LOCAL STORAGE COMMANDS:
SB=: PASS - constrain(atof(value),1.0f,30.0f) correct range, persists to settings.otd.fSetbackTemp.
IT=: PASS - strict 0/1 via checkBoolean(), stores in otIgnoreTransitions.
OH=: PASS - strict 0/1, stores in otOverrideHB.
RS=: PASS - all 8 codes map correctly: HBS->116, HPS->117, WPS->118, WBS->119, HBH->120, HPH->121, WPH->122, WBH->123. Sends WRITE_DATA with value 0.
MI=: PARTIAL - validates 100-2550ms range. PIC spec says 100-1275ms. DEVIATION: upper bound is 2x the PIC limit. This means invalid values 1280-2550 are accepted instead of rejected. Creating audit-fix task.
FS=: PASS - strict 0/1, persists to settings.otd.bFailSafe.

NO-OP COMMANDS (correctly acknowledged):
GA=/GB=: PASS - store GPIO function chars in otGpioFunctions[0/1].
LA=-LF=: PASS - store 6 LED function chars in otLedFunctions[0-5].
VR=: PASS - stores digit in otVoltageRef.
TS=: PASS - stores char in otTempSensor.
FT=: PASS - stores char in otForceThermostat.
DP=: PASS - acknowledged as no-op via synthesizeResponse().

MISSING PIC COMMANDS vs official spec:
BS=: MISSING - PIC spec has BS= (boiler room setpoint, helps convince boilers that ignore thermostat controls). Likely maps to MsgID 16 (TrSet). Creating audit-fix task.
CC=: Implemented in OTDirect (MsgID 7 cooling control) but NOT in PIC spec. This is an OTGWv6.6+ extension. CL= is the PIC-spec cooling command.

PIC spec GW= values 0/1/R: OTDirect extends to 0/1/2/S/M/L/R. Extensions are intentional and documented.

TW=, AX=, DX= commands: NOT in official Schelte Bron firmware.html documentation. Not present in OTDirect. No action needed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All local storage and no-op commands verified. Complete PIC command set cross-checked against Schelte Bron firmware.html.

All commands PASS:
- SB=, IT=, OH=, RS= (all 8 codes correct MsgIDs), FS= local storage commands correct.
- GA=/GB=, LA-LF=, VR=, TS=, FT=, DP= no-op/local-store commands correct.

Deviations found (2 audit-fix tasks created):
1. TASK-186: MI= upper bound 2550ms should be 1275ms per PIC spec.
2. TASK-187: BS= command missing (boiler room setpoint, in PIC spec but not implemented).

Non-issues confirmed:
- RS= all 8 counter codes map correctly to MsgIDs 116-123.
- CC= not in PIC spec but is an OTGW32 extension (reasonable, no action).
- TW=, AX=, DX= not in official PIC spec, not needed.
- GW= extended modes (L, 2, S, M) are intentional OTGW32 extensions beyond PIC spec GW=0/1/R.
<!-- SECTION:FINAL_SUMMARY:END -->
