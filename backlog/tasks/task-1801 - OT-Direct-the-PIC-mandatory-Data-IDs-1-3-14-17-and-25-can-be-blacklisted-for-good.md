---
id: TASK-1801
title: >-
  OT-Direct: the PIC-mandatory Data-IDs 1, 3, 14, 17 and 25 can be blacklisted
  for good
status: To Do
assignee: []
created_date: '2026-11-11T06:39:00Z'
updated_date: '2026-11-12T00:07:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 894000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report A (A3/B3, verified by the coordinator) and report CD (Bug 6). The PIC clears the unknown mark of IDs 0, 1, 3, 14, 17 and 25 on every request (MandatoryID gateway.asm:3383-3388; call sites :2712, :2798, :2959, :3054, :3623-3625). OTD exempts only MsgID 0 from the 3-strike counter (handleMasterResponse OTDirect.ino:1616-1633). At 3 strikes otdIsAltSlot (:1813) turns the thermostat's own frame into an alternative slot, so the thermostat's TSet stops reaching the boiler, and the schedule entry gets disabled = true for the session. clearUnknownCount on a relayed success (:1634-1637) does not clear disabled, so the counter and the flag disagree. Recovery only through AA=, KI=, PM=, GW=R or a mode change (:2537). Scenario: a boiler answers Unknown-DataID to MsgID 1, 17 or 25 three times during a reset or lockout. How often boilers do this was not checked against captures.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Unknown-DataID answers to 1, 3, 14, 17 and 25 never disable these IDs or turn them into alternative slots, as the PIC's MandatoryID
- [ ] #2 A relayed success of an ID clears both its counter and its disabled flag
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (incomplete): covers only the 3-strike path. On the PIC, MandatoryID also undoes a manual UI= for IDs 0,1,3,14,17,25 on every request (G:3409-3416, "even (accidental) manual action cannot blacklist"; call sites G:2720, :2806, :2967, :3651). On OT-Direct UI=1, and also UI=0, goes into otUnknownIds (O:3712-3725) and otdIsAltSlot then takes the thermostat's frame for good (O:1893-1897, :2352-2359). After UI=0 the thermostat status never reaches the boiler. Add an AC for the UI= path including ID 0.
<!-- SECTION:NOTES:END -->
