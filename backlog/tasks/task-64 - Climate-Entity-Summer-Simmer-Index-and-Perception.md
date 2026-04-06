---
id: TASK-64
title: 'Climate Entity: Summer Simmer Index and Perception'
status: To Do
assignee: []
created_date: '2026-04-06 19:12'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/summer_simmer.py
    (full file, SummerSimmer class)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 245-246, attributes; lines 276-279, thermal_comfort usage)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SummerSimmer` calculation to the firmware and expose as climate entity attributes via MQTT. The Summer Simmer Index is a heat-humidity comfort metric: converts temp to Fahrenheit, applies formula `1.98*(F-(0.55-0.0055*H)*(F-58))-56.83`, converts back to Celsius. The perception is a human-readable string (Cool/Slightly Cool/Comfortable/Slightly Warm/Increasing Discomfort/Extremely Warm/Danger Of Heatstroke/etc.). When thermal_comfort is enabled, SAT Python uses the simmer index AS the current temperature for PID control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/summer_simmer_index published with calculated index (float, 1 decimal)
- [ ] #2 MQTT topic sat/summer_simmer_perception published with perception string
- [ ] #3 Formula matches SAT Python exactly: 1.98*(F-(0.55-0.0055*H)*(F-58))-56.83
- [ ] #4 Returns raw temperature when F < 58
- [ ] #5 Perception thresholds match SAT Python (Cool<21.1, Slightly Cool<25.0, Comfortable<28.3, etc.)
- [ ] #6 When thermal_comfort setting enabled, use simmer index as PID input temperature
- [ ] #7 Requires valid humidity reading (returns null otherwise)
<!-- AC:END -->
