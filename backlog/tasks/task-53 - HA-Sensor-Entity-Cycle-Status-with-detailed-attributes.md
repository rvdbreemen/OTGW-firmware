---
id: TASK-53
title: 'HA Sensor Entity: Cycle Status with detailed attributes'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:11'
updated_date: '2026-04-06 20:46'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
    (lines 421-467, SatCycleSensor)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/types.py
    (lines 52-73, CycleKind + CycleClassification)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatCycleSensor` to MQTT auto-discovery with full cycle analytics. The sensor's main state is the CycleClassification (GOOD/OVERSHOOT/UNDERHEAT/SHORT_CYCLING/UNCERTAIN/INSUFFICIENT_DATA). The sensor also exposes 11 extra_state_attributes with detailed cycle metrics including kind, duration, flow temperatures, and tail-end percentile statistics. These attributes need to be published as a JSON payload or individual sub-topics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HA auto-discovery config for cycle_status sensor
- [x] #2 Main state: CycleClassification enum name string
- [x] #3 Attribute: kind (CENTRAL_HEATING/DOMESTIC_HOT_WATER/MIXED/UNKNOWN)
- [x] #4 Attribute: sample_count (int)
- [x] #5 Attribute: duration_seconds (float, rounded to 1 decimal)
- [x] #6 Attribute: max_flow_temperature (float)
- [x] #7 Attribute: fraction_space_heating (float 0-1)
- [x] #8 Attribute: fraction_domestic_hot_water (float 0-1)
- [x] #9 JSON attributes topic published alongside main state for HA to parse
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added SATCycleKind names, fLastCycleDuration, eLastCycleKind, fLastCycleFractionCH/DHW state fields. Published sat/cycle_attributes JSON. Added json_attr_t to cycle_class HA discovery.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cycle status with JSON attributes: kind, samples, duration, max flow, CH/DHW fractions.
<!-- SECTION:FINAL_SUMMARY:END -->
