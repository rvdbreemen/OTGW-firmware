---
id: TASK-1859
title: >-
  OT-Direct: f8.8 conversion truncates (floatToF88) where the PIC rounds;
  commands accept syntax the PIC refuses
status: To Do
assignee: []
created_date: '2026-11-12T05:19:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 935000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while implementing TASK-1842 (RT=), 2026-11-12. OT-Direct converts command values to f8.8 with floatToF88() (src/OTGW-firmware/OTDirect.ino, `(uint16_t)((int16_t)(celsius * 256.0f))`), which truncates the fraction and works on a float parsed by atof(). The PIC parses the text itself: GetFloat (OTGW-other-projects/otgw-6.8/gateway.asm:5118-5185, repo 770aa45) keeps two fraction digits and rounds on the third (:5138-5159), then converts the hundredths to 1/256 steps and rounds UP when not exact (:5169-5178, comment 'the honneywell thermostat always rounds down, so if the result is not exact, round up'); GetPercent adds 'above 100.00 = OR' (:5099-5112); GetPosFloat refuses a minus (:5114-5117). Example: 25.41 -> PIC 0x1969 (tests/heatpump-1.stc R10181969), floatToF88 0x1968; 0.02 -> PIC 0x0006, floatToF88 0x0005.

Affected OT-Direct callers of floatToF88 at dev bf3c581ce (each may differ from the PIC by 1/256 and accept syntax the PIC refuses): BS= (MsgID 16), C2= (8), CC= (7), SW= (56), SH= (57), MM= (14), OT= (27), CL= (cooling level), the MQTT room_temp (24) and room_setpoint (16) paths, setRemoteOverride() for TT=/TC= (MsgID 9), the thermostat-loss setback (checkThermostatTimeout) and the TASK-1836 boot re-send of SW/SH. CS= and the other paths should be checked as well. RT= (TASK-1842) already parses like the PIC with otdPicPercentF88(); a GetFloat counterpart (signed) would cover most of the rest.

Also check per command which PIC parser it uses (GetFloat, GetPosFloat, GetPercent) for the error codes, and the reply text (PIC prints the stored f8.8 value with two decimals).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every OT-Direct command that takes a temperature or percentage converts it like the PIC's parser for that command (GetFloat / GetPosFloat / GetPercent): same f8.8 value, same SE/OR/BV refusals; the list of commands and their PIC parser is in the notes with gateway.asm citations
- [ ] #2 Host test on the real handlers with the PIC's examples (25.41 -> 0x1969, 0.02 -> 0x0006, OT=9.999 -> 10.00, OT=-0.004 -> 0.00 from tests/command-6.stc); red on the old code
- [ ] #3 Replies print the stored value with two decimals, as the PIC
- [ ] #4 Non-command paths (MQTT room_temp/room_setpoint, setback, boot re-send) are either converted the same way or explicitly left as they are with a reason
<!-- AC:END -->
