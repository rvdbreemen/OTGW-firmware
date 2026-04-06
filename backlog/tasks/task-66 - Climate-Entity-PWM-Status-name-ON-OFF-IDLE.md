---
id: TASK-66
title: 'Climate Entity: PWM Status name (ON/OFF/IDLE)'
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
    other-projects/SAT-releases-thermo-nova/custom_components/sat/types.py
    (lines 81-85, PWMStatus)
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/climate.py
    (lines 260-262, PWM attributes)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python PWM status reporting to match the exact attribute pattern. SAT Python exposes three PWM-related attributes on the climate entity: pulse_width_modulation_enabled (bool), pulse_width_modulation_duty_cycle (float), and pulse_width_modulation_state (ON/OFF/IDLE string). The firmware already publishes enabled and duty_cycle, but is missing the PWM state name. IDLE means PWM is not active/applicable, ON means flame-on phase, OFF means flame-off phase.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT topic sat/pwm_state published with PWM status string
- [ ] #2 States: ON (flame-on phase), OFF (flame-off phase), IDLE (PWM not active)
- [ ] #3 Matches SAT Python PWMStatus enum exactly
- [ ] #4 Published alongside existing sat/pwm_duty topic
<!-- AC:END -->
