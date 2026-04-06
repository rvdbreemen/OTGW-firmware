---
id: TASK-68
title: 'Solar Gain: Sun elevation tracking for activation threshold'
status: To Do
assignee: []
created_date: '2026-04-06 19:13'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 504-513, _sun_elevation method)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/solar_gain.py
    (SolarGainController)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (line
    64, CONF_SOLAR_GAIN_MIN_ELEVATION default 12.0)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python sun elevation check for solar gain compensation. SAT Python reads the sun.sun entity's elevation attribute and only activates solar gain when sun elevation > min_elevation (default 12 degrees). This prevents false solar gain triggers during sunrise/sunset when the sun is too low to heat through windows. The firmware currently uses indoor rise rate only. Add sun elevation as an additional gating condition, receivable via MQTT from HA's sun.sun entity.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT subscribe topic sat/sun_elevation for receiving sun elevation from HA
- [ ] #2 Solar gain only activates when sun_elevation > configured minimum (default 12 degrees)
- [ ] #3 MQTT publish sat/solar_gain_sun_elevation for HA display
- [ ] #4 Setting: SATsolarminelev (float, default 12.0 degrees)
- [ ] #5 Existing indoor rise rate check remains as additional condition
- [ ] #6 Graceful fallback: if no sun elevation data, rely on rise rate alone
<!-- AC:END -->
