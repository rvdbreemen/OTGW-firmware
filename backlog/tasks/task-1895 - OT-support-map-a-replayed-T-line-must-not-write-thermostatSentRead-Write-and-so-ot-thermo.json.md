---
id: TASK-1895
title: >-
  OT support map: a replayed T line must not write thermostatSentRead/Write (and
  so /ot-thermo.json)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T09:44:00Z'
updated_date: '2026-11-13T10:46:00Z'
labels:
  - otgw
  - simulation
dependencies: []
priority: medium
ordinal: 965000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-13: data from a simulation run must not stay in what the firmware believes about the real installation. Read-only analysis (d3 workflow wf_a2aa95ff-0f6, trace + independent verifier): the boiler bitmaps are already gated per frame (boilerEvidence = !bLocalAnswer && !bReplayed, OTGW-Core.ino:5955), but the thermostat branch only checks rsptype == OTGW_THERMOSTAT (OTGW-Core.ino:5925). So every T line of an OTGW replay (/simulate/start, restAPI.ino:390, or debug flag otgwsim, debugFlags.h:47) sets thermostatSentRead/Write and thermostatFileDirty, and the 15-minute flush (OTGW-firmware.ino:1020-1022) persists it in /ot-thermo.json, reloaded at boot (loadOtSupportFiles 7416-7464). SAT simulation sends no frames and OT-Direct loopback never produces a T line, so they do not reach this map. Fix (option D, chosen for KISS: no snapshot, no restore, no flash wear, safe across a reboot during a run, real frames during a run still count): add && !OTdata.bReplayed to the fromThermostat test at OTGW-Core.ino:5925 and correct the comment at 5913-5916. Options A (reset at the end), B (LittleFS snapshot/restore) and C (RAM copy/restore) were weighed and rejected (lose real data, or more code and flash writes). Open for Robert: bump the thermostat file format to v3 to clean units that are already polluted. Follow-ups found in the same analysis (separate tasks): replayed MsgID 16 writes SATtargettemp (6010), a replayed PIC banner writes settings.iBoardMode (6227-6229), SAT simulation saves the PID and heating-curve files, RAM-only slave config / iToutsideBoilerMs / otBoilerCache.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: a replayed T line (read and write) leaves thermostatSentRead/Write and thermostatFileDirty unset; red on the current code, green on the fix
- [x] #2 A real (not replayed) T line still sets the bits (regression check in the same test)
- [x] #3 The boiler-side behaviour is unchanged (existing tests/test_boiler_support_bitmap_b_only.py green)
- [x] #4 A v2 /ot-thermo.json is discarded once at boot, v3 is kept; /ot-boiler.json untouched (host test)
- [ ] #5 Bench: one boot with an old v2 /ot-thermo.json on the unit; after boot the file is rewritten as v3 and /ot-boiler.json is unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 Robert chose A (DM 1570749159060013087, reply to the v3 question relayed by the coordinator): bump the thermostat file format to v3, so a v2 /ot-thermo.json is dropped once at the first boot on the new firmware and relearned from live frames; /ot-boiler.json stays. Ships in d3's combined build with TASK-1880 and TASK-1893.

Implemented in wt-connmap (uncommitted until the bump slot): OTGW-Core.ino fromThermostat = rsptype == OTGW_THERMOSTAT && !OTdata.bReplayed (comment updated); loadOtSupportFiles drops thermostat format 1 or 2 and loads only 3; writeOtThermoFile writes v3; header comment documents format 3. Evidence: tests/test_boiler_support_bitmap_b_only.py 10/10 (new: replayed T read/write leaves thermostatSentRead/Write and thermostatFileDirty unset; a real T after a replay still counts), old code fails the replay test; new tests/test_ot_thermo_file_v3.py 5/5 (real loadOtSupportFiles on an in-memory LittleFS: v2 dropped and marked for rewrite, v3 kept, v1 still dropped, /ot-boiler.json v3 read and not dirty; writer pinned to v3), old code fails 3/5. test_fs_update_keep OK. evaluate --quick 85 passed, 0 failed.
<!-- SECTION:NOTES:END -->
