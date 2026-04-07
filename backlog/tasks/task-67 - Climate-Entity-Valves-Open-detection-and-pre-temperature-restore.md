---
id: TASK-67
title: 'Climate Entity: Valves Open detection and pre-temperature restore'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:13'
updated_date: '2026-04-07 16:34'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 415-455, valves_open property)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 237-240, pre_custom/pre_activity attributes)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 800-833, window sensor handler)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port two SAT Python climate entity features: (1) valves_open detection - checks if any controlled TRV/radiator climate entities have open valves by checking their hvac_action or comparing current vs target temp. When no valves are open, PID integral is not updated and error history is not recorded. (2) pre_custom_temperature and pre_activity_temperature - stores the temperature before a preset change or window-open event, so it can be restored when preset is cleared or window closes. These are published as MQTT attributes for HA display.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic sat/valves_open published as bool (true when any valve is demanding heat)
- [x] #2 Valves detection via MQTT input from HA (TRV valve positions or hvac_action)
- [x] #3 When no valves open: skip PID integral update, skip error history recording
- [x] #4 MQTT topic sat/pre_custom_temperature published (temp before preset change)
- [x] #5 MQTT topic sat/pre_activity_temperature published (temp before window-open)
- [x] #6 Temperature restore on preset clear (back to pre_custom) and window close (back to pre_activity)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ACs 1-3 were already implemented from Task #29. This task added pre-temperature tracking:\n\n- OTGW-firmware.h: added fPreCustomTemp and fPreActivityTemp state fields\n- satHandlePreset(): saves fPreCustomTemp before first preset activation, clears on reset to NONE\n- _satCheckWindowTimer(): sets fPreActivityTemp when window timer fires\n- satHandleWindow(): clears fPreActivityTemp on window close\n- satPublishMQTT(): publishes sat/pre_custom_temperature and sat/pre_activity_temperature when non-zero\n\nCommit: 9099b5de"
<!-- SECTION:FINAL_SUMMARY:END -->
