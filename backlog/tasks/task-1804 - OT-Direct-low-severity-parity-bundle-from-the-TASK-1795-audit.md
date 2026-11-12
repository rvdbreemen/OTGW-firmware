---
id: TASK-1804
title: 'OT-Direct: low-severity parity bundle from the TASK-1795 audit'
status: To Do
assignee: []
created_date: '2026-11-11T06:41:00Z'
updated_date: '2026-11-12T00:07:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 897000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. One task for the low candidates; split out any item that grows. Area A: MsgID 16 is not always acknowledged (the PIC always answers WRITE_ACK, gateway.asm:2976-2979, :2688-2693; OTD relays Unknown-DataID and after 3 strikes answers from a slot, OTDirect.ino:1471-1478, :1810-1814); a boiler Data-Invalid does not clear CS/SW/SH (PIC :2822-2825, :3141-3148, :3177-3182; OTD only logs, :1677-1679); a TT/TC override survives a thermostat disconnect (PIC Disconnect :1782-1784; OTD checkThermostatTimeout :2654-2692); CS below 8 C expires on OTD (PIC CommandExpiry :1619-1629); UI= with no slot candidate zeroes the data and sends no boiler frame (:2195-2201 against :3464-3471); an SR= value lands in canonical as a proxy A (:2210-2211, OTGW-Core.ino:5873-5885). Area B: PR=O reports the CS flow setpoint as a room override 'C<value>' (OTDirect.ino:3297-3311 against PrintOverride gateway.asm:2197-2222; the HA climate entity shows a room override of about 55 C while SAT holds CS) (BC3); handlePRresponse ignores M=P/S/L (OTGW-Core.ino:1346-1348); bare error tokens instead of 'XX: BV' (OTDirect.ino:709-713 against gateway.asm:3683-3703) and silent paths: a header shorter than 4 or without '=' (:2813, :2819), PR with an unknown letter or E/F (:3381-3383), an SC= parse failure (:3030), a full ring (:2856, :2931) (BC7); GW=R without a banner and GW=<other> echoed (:3245-3258 against :4734-4738) (BC8); no range checks on CS/C2/MM and negatives accepted (:2839-2850, :2918-2925, :2980 against GetPercentArg :5057-5085) (BC5); a non-numeric argument clears where the PIC gives SE (BC6); the wrong TT/TC comment at :2867-2871 (BC10); MH/MW/M2 answer BV instead of OR (BC11); silent clamping of SB, VS, SW, SH and MI (BC12). Area C/D: the gap after the boiler's answer can be 50-100 ms where the spec's MWT is 100 ms (OpenTherm.cpp:443; OTDirect.ino:1569, :1717, :2237; spec :1100) (Bug 2); MsgID 0 starved for about 8 s by the 60 s slow-poll burst (:1736-1782, :2523-2525) (Bug 4); the fault code ID5 is not read on a new fault (PIC SetPriorityMsg :2751-2759; OTD 60 s poll :188) (Bug 5); no E lines or error counters, error01..04 stay 0 (:673-679, :1680-1689; OTGW-Core.ino:6176-6195) (Bug 7); parity-only frame validation, no stop-bit or mid-bit checks (OpenTherm.cpp:518-538 against gateway.asm:714-717, :959-969) (Bug 8).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Each item fixed, or recorded as intended (a task note, or an ADR where it is a behaviour decision), one line per item in the final summary
- [ ] #2 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #3 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11 added from report E (area E, boiler cache and answer generation): (E-cand 5) SR= answers every matching thermostat frame with READ_ACK (OTDirect.ino:2203-2217 builds 4UL<<28), also a WRITE_DATA; the PIC's messageack returns the request's own type with the response bit, READ -> READ_ACK, WRITE -> WRITE_ACK (gateway.asm:2668-2677); spec v4.2 4.3.1: a WRITE-DATA is answered WRITE-ACK. (E-cand 6) A boiler Data-Invalid neither counts nor resets the Unknown-DataID strike counter (OTDirect.ino:1683-1686; reset only in the SUCCESS branch :1634-1637), while the PIC resets on every non-Unknown reply including Data-Invalid (MsgSupported gateway.asm:2455-2458); so Unknown, Unknown, Data-Invalid, Unknown blacklists on OT-Direct and not on the PIC, and the 'three consecutive' comment at :1788 is not strictly true. Fix the counter or the comment.

PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (MI item worded wrong): OT-Direct does not clamp MI; it rejects <100 with OR and echoes unrounded (O:3813-3822). PIC rounds to 5 ms first, so MI=99 answers `MI: 100` (G:4577-4615, tests/interval-1.stc).
<!-- SECTION:NOTES:END -->
