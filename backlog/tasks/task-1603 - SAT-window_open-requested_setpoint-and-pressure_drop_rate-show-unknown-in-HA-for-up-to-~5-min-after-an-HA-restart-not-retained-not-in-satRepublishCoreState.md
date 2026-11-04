---
id: TASK-1603
title: >-
  SAT: window_open, requested_setpoint and pressure_drop_rate show unknown in HA
  for up to ~5 min after an HA restart (not retained, not in
  satRepublishCoreState)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T13:03:00Z'
updated_date: '2026-11-04T14:52:00Z'
labels:
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 744000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1490 bench (2026-11-04, .61 beta.70): these three SAT states publish with retain=false (SATcontrol.ino:4683, :4707, :4876) from function-static shadows, so the ADR-174 HA-online republish (satRepublishCoreState, SATcontrol.ino:4451) does not reset them; after an HA restart they read unknown until the next heartbeat (~5 min observed for window_open).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After homeassistant/status offline->online the three topics publish within one SAT publish pass (bench: mosquitto_sub timestamps)
- [x] #2 Host or source test shows the three shadows are reset by the HA-online path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reset the function-static shadows of window_open, requested_setpoint and pressure_drop_rate from the HA-online path (move them to file scope next to s_core_* and memset in satRepublishCoreState, or a flag consulted by satPublishMQTT). Source/host test + bench offline->online.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 lead: the first agent pass (3 shadows to file scope) showed ~55 more function-static, non-retained SAT shadows with the same gap (publish calls SATcontrol.ino ~4561-4907). Decision (lead, KISS for the whole class): a uint8 epoch per shadow + g_satShadowEpoch, bumped in satRepublishCoreState(); the helpers treat an old epoch as first-seen. One change in SATmqttPublish covers every SAT state topic; RAM cost absorbed by struct padding (sizeof to be reported).

2026-11-04 (sonnet agent; lead reran in wt-int4): uint8 epoch per SATShadow (sizeof F/I/B/S unchanged 12/12/8/32), g_satShadowEpoch bumped in satRepublishCoreState(); all 9 helpers treat an old epoch as first-seen, outbox deferral still retries. tests/test_sat_mqtt_state.py 30 OK (old: 3 FAIL, epoch_bumped 0 instead of 6), mqtt_contract 15, fallback 8, restart_quiet 20, ot_loss 24 OK; evaluate 0 failed. Known gap: sat/cycle_attributes and sat/climate_attributes JSON blobs refresh only on their heartbeat (<=11 min). Tag beta.71.

2026-11-04 bench beta.73 (.61): mosquitto_pub homeassistant/status offline, then online at 15:49:29: 3 SAT messages in the 20 s before, 153 distinct SAT topics within 8 s after (window_open 15:49:37, requested_setpoint :38, pressure_drop_rate :37, pid_p, room_temp, boiler_status :36-37). No reboot, settings unchanged. Posted msg 1567551972020723812.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Every SAT state topic republishes when Home Assistant comes back online: a uint8 epoch per publish shadow (no size growth), bumped by the ADR-174 HA-online path. Evidence: tests/test_sat_mqtt_state.py 30 OK (old: 3 FAIL), bench .61 beta.73: 153 SAT topics within 8 s of online (3 in the 20 s before). Known gap: the cycle and climate JSON-attribute blobs refresh only on their heartbeat (<=11 min).
<!-- SECTION:FINAL_SUMMARY:END -->
