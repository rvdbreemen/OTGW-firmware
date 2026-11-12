---
id: TASK-1799
title: >-
  OT-Direct: clear values differ from the PIC: MM=- writes 0 %, OT=99 and OT=-
  write an outside temperature
status: To Do
assignee: []
created_date: '2026-11-11T06:37:00Z'
updated_date: '2026-11-12T00:07:00Z'
labels:
  - otdirect
  - review-2026-11-11
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 892000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1795 audit (docs/reviews/2026-11-11_otdirect-pic-parity-audit/). PIC ground truth: other-projects/otgw-6.6/gateway.asm. OT-Direct: src/OTGW-firmware/OTDirect.ino at dev f64aba399. Report B, BC9 (verified by the coordinator) and BC13. MM: the clear test is !isdigit && != '-' (OTDirect.ino:2976), so MM=- takes the write branch; atof('-') = 0 writes MsgID 14 = 0 through enqueueWriteCommand (:2312-2329, refreshed by the scheduler) and replies 'MM: 0'. PIC: non-numeric input, '-' included, clears and replies 'MM: -' (SetMaxModLevel gateway.asm:4752-4765). HA sends exactly MM=- on every unload and stop (opentherm_gw __init__.py:112-115) and for set_max_modulation level -1 (services.py:264-271), so the boiler stays capped at 0 % until a reboot. OT: the PIC clears for 64 C or more and for non-numeric input and replies 'OT: -' (SetOutsideT gateway.asm:4225-4234; pyotgw documents 99 to clear, pyotgw.py:124-141); OTD writes any numeric value to MsgID 27 and to OTcurrentSystemState.Toutside, and OT=- writes 0 C (OTDirect.ino:2988-2999). The range checks of CS/C2/MM (BC5) are in the low bundle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MM=- and any other non-numeric MM clear the override and reply 'MM: -'
- [ ] #2 OT= with a value of 64 or more, and OT=-, clear and reply 'OT: -'; nothing is written to MsgID 27 or to the outside-temperature state
- [ ] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [ ] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (incomplete): OT=+15.4 clears on OT-Direct (test `!isdigit && != '-'`, O:3141-3144). PIC accepts a leading + (GetFloat G:5118-5127) and documents that form (G:45).
<!-- SECTION:NOTES:END -->
