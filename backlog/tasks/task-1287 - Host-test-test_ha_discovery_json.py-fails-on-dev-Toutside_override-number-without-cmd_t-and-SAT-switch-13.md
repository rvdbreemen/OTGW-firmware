---
id: TASK-1287
title: >-
  Host test test_ha_discovery_json.py fails on dev: Toutside_override number
  without cmd_t and SAT switch 13
status: To Do
assignee: []
created_date: '2026-10-22T02:04:00Z'
labels:
  - mqtt
  - ha
  - test
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 415000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1276 (2026-10-22): `python test/host/test_ha_discovery_json.py` compiles the real MQTTHaDiscovery.cpp and prints RESULT: FAIL on a clean HEAD worktree as well, so it is pre-existing and not caused by TASK-1276. The failing parts reported: the Toutside_override number entity has no cmd_t, and SAT switch 13 (overshoot_protection, added in TASK-1268) trips the gate. Sensor payloads all pass. Find whether the test or the discovery payload is wrong, fix it, and make sure the host test is run somewhere (it is not part of evaluate.py or the usual test runs, which is why it went unnoticed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test/host/test_ha_discovery_json.py prints RESULT: PASS on dev, with the root cause of each failure recorded (test vs payload)
- [ ] #2 A broker discovery capture shows the Toutside_override number and SAT switch 13 payloads valid for HA (cmd_t present where HA needs it)
- [ ] #3 The host test is part of a documented test run (evaluate.py gate or the pr-checklist) so a regression shows up
<!-- AC:END -->
