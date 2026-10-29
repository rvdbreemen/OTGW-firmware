---
id: TASK-1481
title: >-
  Host test red on dev: test/host/test_dhw_water_meter.py fails to compile
  (gen_disc_mqtt.inc: 'OTGWcmddropid' undeclared)
status: To Do
assignee: []
created_date: '2026-10-29T15:30:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 641000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by the Taken session 2026-10-29: on a clean origin/dev worktree (f4b8be6d9) test/host/test_dhw_water_meter.py exits 2, COMPILATION FAILED (discovery FIX): gen_disc_mqtt.inc(53) 'OTGWcmddropid' undeclared. Likely the harness stubs miss the discovery id added with the dropped-command feedback (TASK-1439), same class as TASK-1477.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test_dhw_water_meter.py compiles and passes on dev; the fix is in the harness/stubs unless the firmware is wrong
<!-- AC:END -->
