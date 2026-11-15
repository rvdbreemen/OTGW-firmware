---
id: TASK-1750
title: >-
  OT-Direct: MsgID 100 remote override flags like the PIC (TT b'11', OH= high
  byte)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T09:28:00Z'
updated_date: '2026-11-15T07:57:00Z'
labels:
  - otdirect
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 852000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). The PIC answers MsgID 100 with the function bits of the active override: TT= b'11' (manual and program change), TC= b'01' (gateway.asm 6.6:4053-4063, MessageID100 ~3285-3289), copied into the high byte when OH= (OverrideHigh, default on) is set. OT-Direct answers TT= with 0x0002 and never applies OH= (otOverrideHB, OTDirect.ino ~520, ~3566) to the MsgID 100 answer (~1432). Under ADR-223 this answer is the thermostat's only channel, so the flags matter more. Decide with Robert whether to move to the PIC values (behaviour change for thermostats that read MsgID 100).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides: keep 0x0002 or move to PIC parity (TT 0x03, OH= high byte)
- [x] #2 If parity: MsgID 100 answers match gateway.asm for TT=, TC= with OH on/off; host test red before, green after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (AC gap): description says OH defaults on, ACs do not. PIC default IgnoreErr1 and OverrideHigh set (G:5459) -> `PR: T=11` (tests/serial-2.stc). OT-Direct otOverrideHB=0 (O:535) -> T=10.

Decision (Robert, DM 1571426506259963986, reply to the coordinator triage question 5/5 DM 1571283755438448772: "TASK-1750 + TASK-1751 (OT-Direct): MsgID 100 remote-override flags like the PIC (TT b'11', OH= high byte), and TT=0 also clears an MQTT room_setpoint/BS= on MsgID 16. A) both PIC parity, consistent with your 'like the PIC' on 1806. B) keep ADR-223 as is. Recommendation: A"): "A". Reading: PIC parity. PIC source (OTGW-other-projects/otgw-6.8 gateway.asm): SetTempSetPoint :4088-4092 passes b'11' (manual + program change), SetContSetPoint :4083-4087 passes b'01'; SetSetPoint :4096-4098 stores them in the low nibble of RemOverrideFunc; OH= (header :142) copies the low byte of MsgID 100 into the high byte. Implement in lane-95-next with a host test red/green. ADR-223 needs no amendment for this: its Decision already says the MsgID 100 function bits are b'11' for TT and b'01' for TC (gateway.asm:4053-4063 in its citation); OT-Direct's 0x0002 is a deviation from it.

Implemented (lane-95-next): otdOverrideFlagsData(temporary, overrideHigh) in the otdRemoteOverrideReply block of OTDirect.ino: TT= 0x0003, TC= 0x0001, with OH=1 the low byte copied into the high byte (0x0303 / 0x0101); used for the thermostat's MsgID 100 read answer (replyToThermostat) and the master-mode WRITE 100; otdRemoteOverrideReply no longer masks the flags to the low byte. Evidence: tests/test_otd_tt_pic_parity.py (real handleOTDirectCommand, setRemoteOverride, replyToThermostat gateway loop) 46/46 OK on the lane; with OTGW_OTD_INO=HEAD's OTDirect.ino 7 FAIL, of them for this task B2 (TT flags 0x0002 != 0x0003), M1 (master WRITE 100 0x0002), O1 (OH=1 TT -> 0x0303), O2 (OH=1 TC -> 0x0101), O3 (OH=0 TT 0x0003). The OH default (PIC 1, OT-Direct 0) is a separate task.
<!-- SECTION:NOTES:END -->
