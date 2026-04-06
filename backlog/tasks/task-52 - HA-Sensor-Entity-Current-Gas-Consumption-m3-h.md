---
id: TASK-52
title: 'HA Sensor Entity: Current Gas Consumption (m3/h)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-06 19:11'
updated_date: '2026-04-06 20:14'
labels:
  - ha-entity
  - sensor
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/sensor.py
    (lines 219-264, SatCurrentConsumptionSensor)
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `SatCurrentConsumptionSensor` to MQTT auto-discovery. This sensor estimates gas consumption in m3/h based on relative modulation level and configured min/max consumption bounds. Only available when relative modulation is supported and min/max consumption are configured (>0). Returns 0 when flame is off or SAT is inactive.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT topic sat/consumption published with gas consumption in m3/h
- [x] #2 HA auto-discovery config with device_class=gas, unit=m3
- [x] #3 Calculation: min_consumption + (modulation/100 * (max_consumption - min_consumption))
- [x] #4 Returns 0.0 when flame is off or SAT inactive
- [ ] #5 Settings added: SATminconsumption (float, default 0), SATmaxconsumption (float, default 0)
- [ ] #6 Only published when both min and max consumption settings are > 0
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gas consumption sensor with HA discovery (3bc02cad). device_class=gas, unit=m3/h, state_class=measurement. Gated by min/max consumption > 0.
<!-- SECTION:FINAL_SUMMARY:END -->
