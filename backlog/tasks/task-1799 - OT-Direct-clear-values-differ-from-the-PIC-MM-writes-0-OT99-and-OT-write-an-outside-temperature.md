---
id: TASK-1799
title: >-
  OT-Direct: clear values differ from the PIC: MM=- writes 0 %, OT=99 and OT=-
  write an outside temperature
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:37:00Z'
updated_date: '2026-11-15T04:32:00Z'
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
- [x] #1 MM=- and any other non-numeric MM clear the override and reply 'MM: -'
- [x] #2 OT= with a value of 64 or more, and OT=-, clear and reply 'OT: -'; nothing is written to MsgID 27 or to the outside-temperature state
- [x] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (incomplete): OT=+15.4 clears on OT-Direct (test `!isdigit && != '-'`, O:3141-3144). PIC accepts a leading + (GetFloat G:5118-5127) and documents that form (G:45).

2026-11-13 delta parity check (TASK-1851, comms, read-only, OT-Direct at origin/dev 479e49391 vs PIC 6.8 gateway.asm; G=gateway.asm, O=OTDirect.ino): partial: OT=- and OT=+x fixed by 9aa81609c (TASK-1846). Still open: MM=- goes into the numeric branch and writes 0 (O:3433-3436; PIC clears, G:4806-4807); OT>=64 still written (PIC clears, G:4259-4263); clear replies echo the input instead of '-' (G:4264).

95 (2026-11-15, lane-95-1859): PIC 6.8 ground truth SetMaxModLevel gateway.asm 6.8:4794-4807 + GetDecimal :5045-5069 (GetDecimal's OR also returns with carry, so above 255 clears too); SetOutsideT :4256-4270. AC1: MM= rewritten: parse error -> clearWriteOverride(14) + 'MM: -', trailing chars SE, >100 OR, reply PrintByte. AC2: done in TASK-1859 commit a148e41eb (OT= GetFloatArg; parse error or 64.00-127.99 clears with 'OT: -', no write to 27, PS=1 value or Toutside; 128-255 stored as the PIC). AC3: tests/test_otd_pic_float.py (real branches) class MaxModulation + test_OT_clears_like_clr_outside_t: 19/19 green; on 34a23dc25 (code before this fix) MM=- enqueued MsgID 14 = 0 and MM=150 was accepted (2 FAIL); OT on origin/dev 84944a95e: OT=99 enqueued (FAIL). Callers: SAT sends MM=T (clear) and MM=%u (SATcontrol.ino:761-762, :876), unchanged behaviour. Sweep: 37 test_otd_* + sat_command_delivery + prototype_hoisting + test_evaluate + sat_max_modulation_1514 exit 0; evaluate --quick 86/0. AC4 (build + bench) with the 1859 build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in beta.292 (origin/dev 542f95db1). AC1: MM= as SetMaxModLevel (gateway.asm 6.8:4794-4807) with GetDecimal: parse error (MM=-, MM=, minus, >255) clears with 'MM: -', trailing chars SE, >100 OR. AC2: OT= 64+ / OT=- clear with 'OT: -', no write to 27 / PS=1 / Toutside (landed in TASK-1859 beta.290). AC3: tests/test_otd_pic_float.py MaxModulation + OT clear tests on the real branches, green; red on the code before (MM=- wrote 14 = 0, MM=150 accepted; OT=99 wrote on origin). AC4: build beta.294+67e3f51 exit 0, evaluate --quick 86/0, bench 09 on .61 MM items 17-19 PASS.
<!-- SECTION:FINAL_SUMMARY:END -->
