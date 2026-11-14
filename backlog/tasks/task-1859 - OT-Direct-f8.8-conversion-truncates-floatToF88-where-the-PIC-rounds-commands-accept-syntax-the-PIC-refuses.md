---
id: TASK-1859
title: >-
  OT-Direct: f8.8 conversion truncates (floatToF88) where the PIC rounds;
  commands accept syntax the PIC refuses
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T05:19:00Z'
updated_date: '2026-11-14T21:44:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
95 research 2026-11-14 (gateway.asm 6.8 in RvdB/OTGW-other-projects/otgw-6.8, file lines): GetDecimal :5045-5069 integer 0..255, first char not a digit = SE, >255 = OR. GetFloat :5118-5185 optional sign ('-' via GetSignedTemp, '+' skipped), integer by GetDecimal, then only '.' allowed else SE; first two fraction digits kept, the third rounds (>=5 adds 1), further digits only checked for being digits (SE otherwise); hundredths h -> Divide100 :5211-5225 (h*256/100, carry when h=100 adds 1 to the integer) and rounds UP when the remainder is not 0 (:5172-5175). Negative: integer inverted (comf), fraction 100-h, h=0 -> integer+1 (:5160-5167, :5183-5184). GetPosFloat :5114-5117: a '-' is not a sign, so GetDecimal sees '-' -> SE. GetPercent :5099-5112: GetPosFloat, then above 100.00 = OR; parse errors return with DC set (CL uses that, :4454-4462).

Per command (dispatch :3786-3942, :5242): TT SetTempSetPoint :4089 and TC SetContSetPoint :4084 GetPosFloat, 0 clears; SB SetSetBack :4155 GetPosFloat, 0 = SE; SW SetHotWaterTemp :4164 GetFloat, 0 resets, else BV outside dhwsetpointmin..max (MsgID 48); SH SetHeatingTemp :4191 GetFloat, 0 resets, else BV outside chsetpointmin..max (MsgID 49); CS SetCtrlSetpoint :4218 and C2 SetCtrlSetpt2 :5272 GetPercent, 0 clears; OT SetOutsideT :4256 GetFloat, any parse error clears, a value >= 64.00 clears too (bit6 test :4259-4262, the OR return is commented out), negative stored; BS SetFakeSetpoint :4289 GetPercent, 0 = no fake; RT SetRoomTemp :4300 GetPercent (done, TASK-1842); CL SetCoolLevel :4454 GetPercent, parse error clears, OR returned; MM SetMaxModLevel :4794 GetDecimal (integer, TASK-1799 scope). CC= has no PIC command. Replies: CommandFloat -> PrintSigned :2043-2064: negative is negated and '-', a fraction byte 0xFF rounds to the next integer, else hundredths = (frac*100 + 128) >> 8 (round half up via float2 bit7), so 0x0020 prints 0.13 where printf %.2f of 0.125 can print 0.12.
<!-- SECTION:NOTES:END -->
