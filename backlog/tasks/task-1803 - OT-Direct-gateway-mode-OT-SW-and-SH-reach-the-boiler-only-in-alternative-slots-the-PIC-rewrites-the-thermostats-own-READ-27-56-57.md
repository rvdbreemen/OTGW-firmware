---
id: TASK-1803
title: >-
  OT-Direct gateway mode: OT=, SW= and SH= reach the boiler only in alternative
  slots; the PIC rewrites the thermostat's own READ 27/56/57
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:41:00Z'
updated_date: '2026-11-20T01:31:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 896000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report A, A4/A6 (B2). The PIC turns the thermostat's READ 27 into WRITE 27 with the OT= value and answers READ_ACK with that value (MessageID27/UserOutsideTemp gateway.asm:3050-3067). It turns a READ 56/57 into a WRITE once after SW=/SH= (INITSW/INITSH :3116-3121, :3152-3157; also sent in an alternative slot :3419-3424). OTD: applyOverrides never touches a READ (OTDirect.ino:634), MsgID 27 has no override entry (:435-443), and with a thermostat connected queued writes go out only in alternative slots (:1712, :1855-1868). Scenario: with a thermostat whose every Data-ID the boiler supports there are no slots, so OT=8.5 and SW=55 are acknowledged but never reach the boiler, and a weather-compensating thermostat never sees the HA outside temperature. tjfs's stream had about one slot per 30 s (TASK-1707 notes). Not checked: the PIC's MsgID 6 write-permission gate (dhwupdate/maxchupdate) for INITSW/INITSH. Related: TASK-1707 (alternative slots), ADR-224 (MsgID 28 only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With a thermostat in gateway mode, OT= reaches the boiler through the thermostat's own MsgID 27 and the thermostat reads the OT= value
- [x] #2 SW= and SH= turn the thermostat's next READ 56/57 into one WRITE, as the PIC (including its MsgID 6 permission gate)
- [x] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
95 (2026-11-15, lane-95-next): PIC 6.8 (not 6.6) read: MessageID27/UserOutsideTemp gateway.asm:3071-3088 (with OutsideTemp set any request for 27 becomes WRITE with the value, the ack carries the value), MessageID56/57 :3135-3148 / :3171-3184 (READ -> one WRITE while INITSW/INITSH; WRITE always substituted), INITSW/INITSH set by SW=/SH= only when dhwupdate/maxchupdate (MsgID 6 LB bits 0/1, HotWaterTempOK :4181, HeatingTempOK :4208). New '// --- otdRelayRw' block: otdOutsideSet/otdOutsideF88 (OT=, cleared by OT=- and 64+), otdInitSW/otdInitSH with the stored values (set in SW=/SH= when otdBoilerAllowsWrite(MsgID 6 bit)), otdRelayRewriteRead() called from applyOverrides() in gateway mode next to the TASK-1906 hook; buildOverriddenReply() answers a rewritten 27 with the OT= value. A thermostat WRITE 56/57 keeps going through the otOverrides table (already the PIC behaviour). Separate state instead of an otOverrides entry for 27, so OT= does not show as an extra active write override in the web UI. The INITSW flag clears when the rewrite happens (the PIC clears it on the boiler answer; the same unless the frame is lost). Test tests/test_otd_relay_rw.py (real applyOverrides, buildOverriddenReply, OT=/SW=/SH= branches, otdRelayRw block): 3/3; on origin/dev c2cd68b16 the 2 AC tests FAIL (READ 27/56 pass unchanged), the MsgID 6 gate test passes on both. 9 harnesses got stubs. Sweep 53 test_otd_*/sat/pm_chain/hoisting/evaluate + 8 other OTDirect slicers + 3 ps1 harnesses green; evaluate --quick 86/0. AC4 (build + bench) pending: .61 has no thermostat, so the rewrite itself is not benchable there.

Bench 09 on .61 (OT-Direct, no boiler, no thermostat), beta.298+bb38dc5 (bench-only build of lane-95-next on c2cd68b16, 2026-11-15 07:25, exit 0, RAM 130572 B, Flash 2156679 B): regression PASS (OT=7.6 -> WRITE 27 0x079A, OT=- clears, SW=55.7 0x37B4, SH=60). AC4: build exit 0, evaluate --quick 86/0, bench where the hardware allows done; NOT benched (field only): the rewrite of the thermostat's READ 27/56/57 (AC1/AC2) needs a thermostat; host test tests/test_otd_relay_rw.py.

AC review (coordinator rule 2026-11-15): AC1 and AC2 ask for live behaviour with a thermostat; they are proven by tests/test_otd_relay_rw.py only, so they stay open as field ACs. AC3 (host test) and AC4 (build/evaluate/bench where possible, field part named) stay ticked.

Landed: Pushed origin/dev 2e4ca9c05..3e29faa63; zip beta.301+2b3d9a2 posted (Discord 1571471374038470711); final build exit 0 'Build completed successfully', RAM 130572 B, Flash 2157483 B; sweep 88/88, evaluate --quick 86/0; bench 09 on .61 beta.301+2b3d9a2 PASS (smoke + re-check). Status In Review (convention: field ACs open, see AC list; host-proven).

2026-11-20 field archive check (otgw-dev, read-only): tjfs is the only OT-Direct field user. His two beta.161 captures (OTGW-field-data/tjfs/2026-11-09_*) contain no OT=, SW= or SH= command (transcript grep 'cmd "(OT|SW|SH)='). The thermostat does read MsgID 27 (3x) and 57 (2x), none 56, in 20261109-093934. Without a command there is no rewrite to see, so AC1/AC2 have no field evidence; they stay open as field ACs (list for Robert: waiver or 2.1.0).

2026-11-20 field waiver: Robert, Discord DM 1573146152524587149 ('A', reply to the coordinator's question 1573145337458065529, relayed by the coordinator): close with a field waiver and one 2.1.0 follow-up task per field AC. AC1 waived per Robert DM 1573146152524587149, follow-up TASK-2942. AC2 waived per Robert DM 1573146152524587149, follow-up TASK-2943. Ticked as waived, not met in the field: evidence host-only (tests/test_otd_relay_rw.py) plus the .61 regression bench; tjfs's captures have no OT=/SW=/SH= command (note above).
<!-- SECTION:NOTES:END -->
