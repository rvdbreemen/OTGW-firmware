---
id: TASK-1481
title: >-
  Host test red on dev: test/host/test_dhw_water_meter.py fails to compile
  (gen_disc_mqtt.inc: 'OTGWcmddropid' undeclared)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T15:30:00Z'
updated_date: '2026-10-29T19:45:00Z'
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
- [x] #1 test_dhw_water_meter.py compiles and passes on dev; the fix is in the harness/stubs unless the firmware is wrong
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stale harness, firmware right. test/host/test_dhw_water_meter.py exited 2 on dev (COMPILATION FAILED (discovery FIX): gen_disc_mqtt.inc(53) 'OTGWcmddropid' undeclared): queueNonOTDiscoveryIds() queues faux id 240 since TASK-1439, which the harness FAUX_IDS list lacked. After adding it the run reached the mutant stage and exited 1 on a stale anchor (M6 expected MQTT_HA_SENSOR_COUNT = 393; it is 394 since TASK-1439); anchor updated to 394 -> 393 (same mutant meaning: count one short). Re-run: exit 0, RESULT: PASS, every mutant killed. Test file only, no bump.
<!-- SECTION:FINAL_SUMMARY:END -->
