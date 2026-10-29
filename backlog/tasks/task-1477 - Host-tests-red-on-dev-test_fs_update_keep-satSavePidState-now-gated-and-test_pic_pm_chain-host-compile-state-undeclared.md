---
id: TASK-1477
title: >-
  Host tests red on dev: test_fs_update_keep (satSavePidState now gated) and
  test_pic_pm_chain (host compile: 'state' undeclared)
status: To Do
assignee: []
created_date: '2026-10-29T14:10:00Z'
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
- [ ] #1 test_fs_update_keep.py green with an assertion that matches entry 115 (gated save), red if the gate is removed
- [ ] #2 test_pic_pm_chain.py compiles and passes on dev
<!-- AC:END -->
