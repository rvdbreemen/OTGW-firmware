---
id: TASK-1603
title: >-
  SAT: window_open, requested_setpoint and pressure_drop_rate show unknown in HA
  for up to ~5 min after an HA restart (not retained, not in
  satRepublishCoreState)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T13:03:00Z'
updated_date: '2026-11-04T14:15:00Z'
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
- [ ] #1 After homeassistant/status offline->online the three topics publish within one SAT publish pass (bench: mosquitto_sub timestamps)
- [ ] #2 Host or source test shows the three shadows are reset by the HA-online path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Reset the function-static shadows of window_open, requested_setpoint and pressure_drop_rate from the HA-online path (move them to file scope next to s_core_* and memset in satRepublishCoreState, or a flag consulted by satPublishMQTT). Source/host test + bench offline->online.
<!-- SECTION:PLAN:END -->
