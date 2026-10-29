---
id: TASK-1477
title: >-
  Host tests red on dev: test_fs_update_keep (satSavePidState now gated) and
  test_pic_pm_chain (host compile: 'state' undeclared)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T14:10:00Z'
updated_date: '2026-10-29T14:58:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 637000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1357 (2026-10-29), red on HEAD c8425f9b2 too, so pre-existing: tests/test_fs_update_keep.py expects satSavePidState() unconditionally in satPersistAllToFs, but SATcontrol.ino now has 'if (satPidRoomSeen()) satSavePidState();' (entry 115, TASK-1365); tests/test_pic_pm_chain.py fails to host-compile with "'state' undeclared". Decide per test whether the test or the code is wrong (entry 115 says gated is intended) and make both green.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 test_fs_update_keep.py green with an assertion that matches entry 115 (gated save), red if the gate is removed
- [x] #2 test_pic_pm_chain.py compiles and passes on dev
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Both tests were stale, the code is right.
- AC#1 test_fs_update_keep.py: satPersistAllToFs gates the PID save on satPidRoomSeen() by design (Sergeant D entry 115, TASK-1365); the old test demanded an unconditional call. test_calls_every_saver now skips satSavePidState, and the new test_pid_save_gated_on_room_seen asserts the gate. Ran 24 OK; with the gate removed from the source text (monkeypatched SATCTL) the new test FAILS (failures=1).
- AC#2 test_pic_pm_chain.py: the cut-out range of OTGW-Core.ino now includes the dropped-command feedback from TASK-1439 (state.pic, CCONOFF, sendMQTTData), which the stubs lacked. Stubs added; Ran 9 OK (host compile succeeds).
Test files only, no firmware change, no bump.
<!-- SECTION:FINAL_SUMMARY:END -->
