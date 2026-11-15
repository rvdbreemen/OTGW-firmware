---
id: TASK-1803
title: >-
  OT-Direct gateway mode: OT=, SW= and SH= reach the boiler only in alternative
  slots; the PIC rewrites the thermostat's own READ 27/56/57
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T06:41:00Z'
updated_date: '2026-11-15T06:18:00Z'
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
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
95 (2026-11-15, lane-95-next): PIC 6.8 (not 6.6) read: MessageID27/UserOutsideTemp gateway.asm:3071-3088 (with OutsideTemp set any request for 27 becomes WRITE with the value, the ack carries the value), MessageID56/57 :3135-3148 / :3171-3184 (READ -> one WRITE while INITSW/INITSH; WRITE always substituted), INITSW/INITSH set by SW=/SH= only when dhwupdate/maxchupdate (MsgID 6 LB bits 0/1, HotWaterTempOK :4181, HeatingTempOK :4208). New '// --- otdRelayRw' block: otdOutsideSet/otdOutsideF88 (OT=, cleared by OT=- and 64+), otdInitSW/otdInitSH with the stored values (set in SW=/SH= when otdBoilerAllowsWrite(MsgID 6 bit)), otdRelayRewriteRead() called from applyOverrides() in gateway mode next to the TASK-1906 hook; buildOverriddenReply() answers a rewritten 27 with the OT= value. A thermostat WRITE 56/57 keeps going through the otOverrides table (already the PIC behaviour). Separate state instead of an otOverrides entry for 27, so OT= does not show as an extra active write override in the web UI. The INITSW flag clears when the rewrite happens (the PIC clears it on the boiler answer; the same unless the frame is lost). Test tests/test_otd_relay_rw.py (real applyOverrides, buildOverriddenReply, OT=/SW=/SH= branches, otdRelayRw block): 3/3; on origin/dev c2cd68b16 the 2 AC tests FAIL (READ 27/56 pass unchanged), the MsgID 6 gate test passes on both. 9 harnesses got stubs. Sweep 53 test_otd_*/sat/pm_chain/hoisting/evaluate + 8 other OTDirect slicers + 3 ps1 harnesses green; evaluate --quick 86/0. AC4 (build + bench) pending: .61 has no thermostat, so the rewrite itself is not benchable there.
<!-- SECTION:NOTES:END -->
