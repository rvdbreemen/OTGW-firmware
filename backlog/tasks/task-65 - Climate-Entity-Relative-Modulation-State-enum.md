---
id: TASK-65
title: 'Climate Entity: Relative Modulation State enum'
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
    other-projects/SAT-releases-thermo-nova/custom_components/sat/types.py
    (lines 102-107, RelativeModulationState)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 258-259, modulation state attribute)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `RelativeModulationState` enum to the firmware and publish as a climate attribute. SAT Python tracks the modulation controller state as one of: OFF (modulation control disabled), COLD (boiler below cold threshold), PWM_OFF (PWM mode, flame off phase), HOT_WATER (DHW active, modulation not controlled). This provides context for why modulation is at a certain level.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/modulation_state published with state name string
- [ ] #2 States: OFF, COLD, PWM_OFF, HOT_WATER (matching SAT Python RelativeModulationState)
- [ ] #3 OFF when modulation control is disabled or SAT is off
- [ ] #4 COLD when boiler temp < cold setpoint (22C default)
- [ ] #5 PWM_OFF during PWM flame-off phase
- [ ] #6 HOT_WATER when DHW is active
<!-- AC:END -->
