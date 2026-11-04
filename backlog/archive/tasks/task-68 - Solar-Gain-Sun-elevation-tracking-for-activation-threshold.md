---
id: TASK-68
title: 'Solar Gain: Sun elevation tracking for activation threshold'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:13'
updated_date: '2026-04-07 16:35'
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
- [x] #1 MQTT subscribe topic sat/sun_elevation for receiving sun elevation from HA
- [x] #2 Solar gain only activates when sun_elevation > configured minimum (default 12 degrees)
- [x] #3 MQTT publish sat/solar_gain_sun_elevation for HA display
- [x] #4 Setting: SATsolarminelev (float, default 12.0 degrees)
- [x] #5 Existing indoor rise rate check remains as additional condition
- [x] #6 Graceful fallback: if no sun elevation data, rely on rise rate alone
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added sun elevation gating for solar gain compensation to prevent false triggers during sunrise/sunset.

Changes:
- OTGW-firmware.h: Added fSolarMinElevation setting (default 12.0 degrees) to SATSection; added fSunElevation, bSunElevationValid, iSunElevLastMs state fields to SATState
- SATcontrol.ino: Added satHandleSunElevation() handler; modified satUpdateSolarGain() to gate on sun elevation before rise rate logic (stale check at 1 hour); added sat/solar_gain_sun_elevation MQTT publish when elevation is valid
- MQTTstuff.ino: Added sat/sun_elevation subscriber handler and sat/solar_min_elevation setting command
- settingStuff.ino: Added SATsolarminelev save (writeJsonFloatKV) and load (constrain -10 to 45 degrees) persistence

Behavior:
- If valid sun elevation data exists and elevation < fSolarMinElevation, solar gain is disabled regardless of rise rate
- Stale elevation data (>1 hour) reverts to rise-rate-only mode for graceful fallback
- If no sun elevation data has ever been received, existing rise rate logic is used unchanged
<!-- SECTION:FINAL_SUMMARY:END -->
