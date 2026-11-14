---
id: TASK-1944
title: >-
  OT-Direct: at boot the restored HW= and UI= list are wiped again when the
  auto-detect switches to master
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T18:07:00Z'
updated_date: '2026-11-14T19:15:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by d3 while doing TASK-1839 (2026-11-14), read from code only, not reproduced on a bench. initOTDirect() restores the persisted command state first (otdRestorePersistedCommands(), TASK-1836: HW, SW, SH, IT, OH and the UI= list) and only then runs the thermostat auto-detect; when no thermostat is found it calls applyOTDirectMode(OTD_MODE_MASTER) (the 'not persisted: the next boot detects again' line). A switch into master keeps the full resetTransientState() (TASK-1839 scope), which sets otDHWOverride back to auto (0xFF), otMasterStatusFlags to 0x01, clears otUnknownIdCount and the schedule disable flags. So a persisted HW=0 or UI= list is lost on every boot of a standalone OTGW32 with auto-detect on. SW/SH are re-sent through the queue before the switch; the reset flushes the queue too (ADR-223), so those boot writes may be dropped as well (to verify).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A boot with auto-detect on and no thermostat keeps the persisted HW=, UI= list, SW= and SH= (host test on the real initOTDirect order or the real reset, red on current code)
- [x] #2 The order is decided: restore after the auto-detect, or the boot switch flushes only the queue; recorded in the task notes
- [ ] #3 Bench via 09 on an OTGW32 with no thermostat: HW=0 and a UI= entry survive a reboot
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Order decided (AC2): restore after the auto-detect. initOTDirect() moves the TASK-584 vent restore (updateWriteCache/setOverride 71) and otdRestorePersistedCommands() (HW, SW, SH, IT, OH, UI=) from before the bus probe to after the auto-detect block (after a possible applyOTDirectMode(OTD_MODE_MASTER) and its full reset) and before otdBootHandshake(). Nothing between uses the restored state (step-up, bus probe, master/slave begin, HW_MODE set, the 5 s detect loop only watches otSlaveFramePending, sends nothing). Alternative rejected: a flush-only boot switch into master: it would make the boot switch differ from a user GW=S switch, and master's reset also clears state that is meaningless at boot anyway. Test: structural on the real initOTDirect order (restore after the auto-detect switch, before the handshake), red on current code; plus the existing OTD suite. Bench via 09 on .61 (no thermostat): HW=0 and a UI= entry survive a reboot.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Citations (lane-1924 941a224f9): src/OTGW-firmware/OTDirect.ino:1202 initOTDirect(), :1236 otdRestorePersistedCommands(), :1311 applyOTDirectMode(OTD_MODE_MASTER) after the auto-detect finds no thermostat; resetTransientState() clears HW/UI at :2886-2896 region (otDHWOverride = 0xFF, otMasterStatusFlags = 0x01, otUnknownIdCount = 0, schedule disabled flags).

Implemented (d3, 2026-11-14, local, placeholder beta.254, not yet built). initOTDirect(): the TASK-584 vent restore and otdRestorePersistedCommands() moved from before the bus probe to after the auto-detect block and before otdBootHandshake() (order decided, see plan). Also the vent setpoint (write cache + override 71) was lost by the master reset, now kept too. tests/test_otd_boot_restore_order.py (source order on the real initOTDirect, which waits 5 s on hardware and cannot run on the host): 3/3 on the fix, 2 FAIL on HEAD. OTD suite + init-related tests 41/41, evaluate --quick exit 0. Not verified: whether the SW/SH boot writes were also dropped by the queue flush before (they are enqueued after the switch now, so the question is moot). AC3 (bench) open.
<!-- SECTION:NOTES:END -->
