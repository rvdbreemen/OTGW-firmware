---
id: TASK-72
title: 'Climate Entity: Additional extra_state_attributes for full parity'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:14'
updated_date: '2026-04-08 21:41'
labels:
  - ha-entity
  - climate
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 236-263, extra_state_attributes property)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add the remaining SAT Python climate entity extra_state_attributes that are not yet covered by other tasks. SAT Python exposes 35 attributes on the climate entity. This task covers the attributes not addressed by tasks #64-67: optimal_coefficient, coefficient_derivative, minimum_setpoint, boiler_flame_timing, boiler_temperature_cold, boiler_temperature_tracking, boiler_temperature_derivative, error_source, error_pid, integral_enabled, derivative_enabled, derivative_raw, current_kp/ki/kd, relative_modulation_enabled. Most of these map to existing firmware state fields but need to be published as a JSON attributes blob.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 JSON attributes topic sat/climate_attributes published with all remaining attributes
- [x] #2 Includes: optimal_coefficient, coefficient_derivative from heating curve auto-tune
- [x] #3 Includes: minimum_setpoint (dynamic or static current value)
- [x] #4 Includes: boiler_flame_timing (average flame on-time seconds)
- [x] #5 Includes: boiler_temperature_cold, boiler_temperature_tracking, boiler_temperature_derivative
- [x] #6 Includes: error_source (which zone has max error), error_pid, integral_enabled, derivative_enabled
- [x] #7 Includes: derivative_raw, current_kp, current_ki, current_kd
- [x] #8 Includes: relative_modulation_enabled (bool)
- [x] #9 Climate HA auto-discovery uses json_attributes_topic for these
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add sat/climate_attributes JSON blob publish at end of satPublishMQTT() in SATcontrol.ino
2. Map each required attribute to existing state/settings fields or sensible defaults
3. Update mqttha.cfg climate discovery entry to include json_attributes_topic
4. Check all ACs and add final summary
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Published sat/climate_attributes JSON attributes blob and wired it into HA climate auto-discovery.

Changes:
- SATcontrol.ino: Added Task #72 block at end of satPublishMQTT() (before weatherPublishMQTT()) that builds and publishes a single JSON object on sat/climate_attributes containing 16 fields.
- mqttha.cfg: Added "json_attributes_topic": "%mqtt_pub_topic%/sat/climate_attributes" to the SAT climate entity discovery config (line 502).

Field mapping:
- optimal_coefficient: settings.sat.fHeatingCurveCoeff (heating curve coefficient is the SAT Python equivalent)
- coefficient_derivative: hardcoded 0.0 (not tracked in firmware)
- minimum_setpoint: SAT_MIN_SETPOINT constant (10.0 C)
- boiler_flame_timing: state.sat.fLastCycleDuration (duration of last completed flame cycle in seconds)
- boiler_temperature_cold: OTcurrentSystemState.Tboiler (current boiler temperature; proxy for cold reference)
- boiler_temperature_tracking: hardcoded false (no EMA boiler temp tracking in firmware)
- boiler_temperature_derivative: hardcoded 0.0 (not tracked)
- error_source: hardcoded "main" (single zone, no multi-zone concept)
- error_pid: state.sat.fError (current target - room error)
- integral_enabled: hardcoded true (always active when SAT runs)
- derivative_enabled: hardcoded true (always active when SAT runs)
- derivative_raw: state.sat.fRawDerivative (adaptive low-pass filtered derivative)
- current_kp, current_ki, current_kd: state.sat.fKp/fKi/fKd (auto-calculated gains)
- relative_modulation_enabled: derived from satGetManufacturerQuirks() & SAT_QUIRK_NO_REL_MOD

Implementation uses a 512-byte static char buffer with incremental snprintf_P/dtostrf calls. No ArduinoJson, no String class. Buffer usage is approximately 440 bytes max.
<!-- SECTION:FINAL_SUMMARY:END -->
