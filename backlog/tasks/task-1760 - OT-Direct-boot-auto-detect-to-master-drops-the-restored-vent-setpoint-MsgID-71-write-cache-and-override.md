---
id: TASK-1760
title: >-
  OT-Direct: boot auto-detect to master drops the restored vent setpoint (MsgID
  71 write cache and override)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T18:35:00Z'
updated_date: '2026-11-14T22:36:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 857000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1752 review 2026-11-09 (code reading, not reproduced): initOTDirect() restores the vent setpoint via updateWriteCache(71) and setOverride(71) (~OTDirect.ino:920), then the no-thermostat auto-detect calls applyOTDirectMode(OTD_MODE_MASTER) (~1076), whose resetTransientState() clears all write caches (valueSet=false) and otOverrides. So on a board without a thermostat the persisted vent setpoint is not written after boot. Needs a host test on the real initOTDirect/resetTransientState slices, and a check of which other boot restores are lost the same way.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real code shows whether the restored MsgID 71 write survives the boot auto-detect to master (red on current code if lost)
- [x] #2 If lost: the restore runs after the mode is final; test green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PIC 6.8 parity audit 2026-11-12 (docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md), section 3 'Corrections to existing tasks'. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = OTDirect.ino at fa5790118. Read from code only. Correction (premise wrong): boot restore of the vent setpoint is a no-op in every mode, not only after auto-detect. MsgID 71 has no write entry (O:159-167; only R_ENTRY(71) at O:217) and no override slot (O:434-442), so updateWriteCache(71)/setOverride(71) at O:1018-1024 do nothing. See N1. Fix and boot restore are in TASK-1831 (N1, AC 3).

2026-11-13 delta parity check (TASK-1851, comms, read-only, OT-Direct at origin/dev 479e49391 vs PIC 6.8 gateway.asm; G=gateway.asm, O=OTDirect.ino): premise now correct (TASK-1831 added the MsgID 71 write entry). Scope is wider: with auto-detect on, a gateway boot with no thermostat runs the restore (O:1217-1223) and then applyOTDirectMode(MASTER) (O:1298), whose resetTransientState() wipes HW (O:2867, :2871), the UI= list and schedule disabled flags (O:2875-2879), the queued SW-boot/SH-boot frames (ring flush O:2846-2847), the SW/SH write values and overrides (O:2885-2890) and VS. So TASK-1836 (Done) does not hold its AC2 on that path. Extend 1760 rather than a new task.

Covered by TASK-1944 (d3, lane-1924 commit b3e17f0d2, not pushed yet): initOTDirect() now runs the vent restore (updateWriteCache(71)/setOverride(71)) and otdRestorePersistedCommands() (HW, SW, SH, IT, OH, UI= list, TASK-1836) after the auto-detect switch to master and before otdBootHandshake(). That is the whole scope listed in the 2026-11-13 delta check (HW, UI=, SW/SH values and overrides, VS). The queued SW-boot/SH-boot frames come from the restore itself, so they are queued after the flush now. Evidence AC1/AC2: tests/test_otd_boot_restore_order.py (source order on the real initOTDirect(); initOTDirect blocks 5 s on OT hardware and cannot run on the host): 3/3 OK on the fix, 2 FAIL on the code before 1944; the reset itself runs on the real code in tests/test_otd_mode_change_keeps_state.py. Bench of the boot path on .61 is in the lane bench request (TASK-1944 AC3). Close together with TASK-1944 after the push.
<!-- SECTION:NOTES:END -->
