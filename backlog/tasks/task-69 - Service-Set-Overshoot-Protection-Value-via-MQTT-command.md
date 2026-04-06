---
id: TASK-69
title: 'Service: Set Overshoot Protection Value via MQTT command'
status: To Do
assignee: []
created_date: '2026-04-06 19:13'
labels:
  - ha-entity
  - service
  - sat-parity
milestone: SAT HA Entity Parity
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (line
    202, SERVICE_SET_OVERSHOOT_PROTECTION_VALUE)
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the SAT Python `set_overshoot_protection_value` service. SAT Python allows setting the OPV value directly via a HA service call, in addition to the automatic calibration. The firmware already has OPV calibration and an OPV value setting, but it should also accept an MQTT command to set the value directly (like the existing ovp_value MQTT subscribe topic). Verify this is fully working and add HA service-equivalent documentation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTT command topic set/.../sat/ovp_value accepts direct OPV value input
- [ ] #2 Value range validation (0-10C)
- [ ] #3 Value persists to settings immediately
- [ ] #4 REST API endpoint POST /api/v2/sat/ovp_value also accepts direct value
- [ ] #5 Documentation in mqttha.cfg comments for HA users
<!-- AC:END -->
