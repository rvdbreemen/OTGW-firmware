---
id: TASK-3129
title: >-
  SAT settings state echo (sat/* static block incl. weather_lat/lon, zone_count)
  is never published while SAT is held back by the boot, OT-link or
  outside-temperature gate
status: To Do
assignee: []
created_date: '2026-11-27T18:36:00Z'
labels:
  - bug
  - sat
  - mqtt
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1180000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the B2 bench 2026-11-27 (TASK-3016/3102 echo check). satPublishMQTT() (SATcontrol.ino:5644), which holds the whole static sat/* settings block (valve_offset, min_pressure, presets, weather_lat/lon, zone_count), is only called from the control tick (:8494, :8577, :8607, :8623, :8988) and the retry (:8351). The tick returns before DUE(timerSATControl) (:8439) on three gates: satBootSilenceStep (60 s after boot), satOtLinkStep (no boiler reply for 30 s) and satRestartStep (no outside temperature younger than 1 h, :4838-4849). While a gate holds, state.sat.bActive stays false and no static topic is published. Old behaviour (block since 650aba821, 2026-04-07), not a B2 regression. Evidence: .63 800 s subscribe on # (2562 msgs, 600 topics) has zero sat/weather_lat, weather_lon, zone_count; telnet captures beta.401..458 show 0 static-block lines and 0 sat/setpoint, pid_output, error, target; .63 ot_link_lost=true (111x) with outside ok; .76 enabled=true, ot_link_lost=false, active=false, outside_temp_missing=true (8x), no weather or external temp source; .63 with simulation on shows 136 sat/ topics live. Smallest fix idea: a 30 s timer at SATcontrol.ino:8351 that calls satPublishMQTT() while satIsEnabled() and not bActive. Risk: publishes SAT state while the boiler is handed back. Open design question for Robert and otgw-poc: is a settings echo wanted while SAT is handed back (not control behaviour, so no Sergeant D question).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test in the tests/sat_loop_build.py harness (stale outside temperature, SAT on, 120 s of loop) sees at least one sat/zone_count publish; red on the current code (0), green on the fix
- [ ] #2 A unit with SAT enabled but no boiler reply or no outside temperature publishes the static sat/* block within 60 s after the boot silence ends (bench evidence on .63 and .76)
- [ ] #3 The echo while handed back is decided (publish or document) and docs/api/MQTT.md states when the sat/* state topics appear
<!-- AC:END -->
