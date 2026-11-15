---
id: TASK-1801
title: >-
  OT-Direct: the PIC-mandatory Data-IDs 1, 3, 14, 17 and 25 can be blacklisted
  for good
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:39:00Z'
updated_date: '2026-11-15T04:32:00Z'
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
- [x] #1 Unknown-DataID answers to 1, 3, 14, 17 and 25 never disable these IDs or turn them into alternative slots, as the PIC's MandatoryID
- [x] #2 A relayed success of an ID clears both its counter and its disabled flag
- [x] #3 Host test against the real code (not a re-implementation): red on the old code, green on the fix
- [x] #4 esp32-combo build, evaluate.py --quick, bench check where the hardware allows (name what was not benched)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (incomplete): covers only the 3-strike path. On the PIC, MandatoryID also undoes a manual UI= for IDs 0,1,3,14,17,25 on every request (G:3409-3416, "even (accidental) manual action cannot blacklist"; call sites G:2720, :2806, :2967, :3651). On OT-Direct UI=1, and also UI=0, goes into otUnknownIds (O:3712-3725) and otdIsAltSlot then takes the thermostat's frame for good (O:1893-1897, :2352-2359). After UI=0 the thermostat status never reaches the boiler. Add an AC for the UI= path including ID 0.

2026-11-13 delta parity check (TASK-1851, comms, read-only, OT-Direct at origin/dev 479e49391 vs PIC 6.8 gateway.asm; G=gateway.asm, O=OTDirect.ino): still valid and worse: TASK-1836 now persists and restores the UI= list, and the restore accepts 0-127 (O:3224-3242, UI handler O:4021-4025), so an accidental UI=0/UI=1 survives a reboot; the PIC undoes it for the mandatory IDs on every request (G:3409-3416 per REVIEW).

95 (2026-11-15, lane-95-1859): PIC 6.8 ground truth MandatoryID gateway.asm 6.8:3409-3416 (unmarks on every request, also a manual mark), call sites 0 readstatus :2720, 1 MessageID1 :2806, 14 MessageID14 :2967, 3/17/25 MandatoryWord :2544/:2558/:2566/:3651; 27 only with an external sensor as alternative (:3071-3075), not applicable on OT-Direct. Fix: '// --- otdUnknownStrike' block (otdPicMandatoryId, otdNoteBoilerAnswer) above handleMasterResponse(), which now calls it; otdIsAltSlot() returns false for a mandatory ID (also against a UI= mark). AC2: a non-Unknown answer clears the counter and enables the schedule entry again, but only when the strikes disabled it (counter at 3) and the ID has no UI= mark, so a user DA= or UI= is not undone (both also set disabled, OTDirect.ino DA= ~4087, UI= ~4211, otdRestoreUnknownIds ~3385). Relayed thermostat requests go through sendMasterRequestAsync(..., OT_DIRECT_ORIGIN_THERMOSTAT), so their answers reach handleMasterResponse() too. AC3: tests/test_otd_mandatory_ids.py (real counters, isUnknownId, block, otdIsAltSlot; on the old revision the inline block of handleMasterResponse is cut and wrapped as is): 5/5 green; on origin/dev 84944a95e 3 FAIL (mandatory IDs disabled + alt after 3 strikes, UI=14 made 14 an alternative, success left 5 disabled), the 2 guard tests pass on both. Block placed above handleMasterResponse so test_otd_master_empty_slot's counter slice stays unchanged. Sweep: 38 test_otd_* + sat_command_delivery + prototype_hoisting + test_evaluate + host local_frame_consumers + boiler_unsupported_origin exit 0; ps1 unknown_counters and loopback PASS; evaluate --quick 86/0. AC4 with the 1859 build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in beta.293 (origin/dev 542f95db1). AC1: Unknown-DataID answers to 1, 3, 14, 17, 25 (0 already) never disable them or make them alternative slots, also not with a UI= mark (otdUnknownStrike block, otdIsAltSlot), as MandatoryID gateway.asm 6.8:3409-3416. AC2: a non-Unknown answer clears the counter and enables an entry the strikes disabled; DA=/UI= stay. AC3: tests/test_otd_mandatory_ids.py on the real counters/block/otdIsAltSlot, 5/5 green, 3 FAIL on origin/dev (old inline block wrapped as is). AC4: build beta.294+67e3f51 exit 0, evaluate 86/0; NOT benched: .61 has no boiler that answers Unknown-DataID, so the live strike path is host-test only (named in the bench brief).
<!-- SECTION:FINAL_SUMMARY:END -->
