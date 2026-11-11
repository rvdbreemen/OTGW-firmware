---
id: TASK-1803
title: >-
  OT-Direct gateway mode: OT=, SW= and SH= reach the boiler only in alternative
  slots; the PIC rewrites the thermostat's own READ 27/56/57
status: To Do
assignee: []
created_date: '2026-11-11T06:41:00Z'
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
- [ ] #1 With a thermostat in gateway mode, OT= reaches the boiler through the thermostat's own MsgID 27 and the thermostat reads the OT= value
- [ ] #2 SW= and SH= turn the thermostat's next READ 56/57 into one WRITE, as the PIC (including its MsgID 6 permission gate)
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->
