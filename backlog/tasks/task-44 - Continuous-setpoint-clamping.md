---
id: TASK-44
title: Continuous setpoint clamping
status: To Do
assignee: []
created_date: '2026-04-05 21:06'
labels:
  - sat
  - feature
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/heating_control.py:450-509
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement asymmetric setpoint clamping for continuous modulation mode. Different behavior for rising vs falling demand. minimum_allowed = boiler_temp - flow_offset (default 2.0C). Prevents setpoint spikes when boiler reports overheat temperature, ensuring smooth control transitions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setpoint clamping applies different logic for rising vs falling demand
- [ ] #2 minimum_allowed calculated as boiler_temp - flow_offset (default 2.0C)
- [ ] #3 Prevents setpoint spikes on boiler overheat conditions
- [ ] #4 flow_offset configurable (default 2.0C)
- [ ] #5 Clamping logic only active in continuous modulation mode
<!-- AC:END -->
