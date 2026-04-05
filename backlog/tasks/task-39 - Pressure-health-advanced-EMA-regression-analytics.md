---
id: TASK-39
title: 'Pressure health: advanced EMA + regression analytics'
status: To Do
assignee: []
created_date: '2026-04-05 21:05'
labels:
  - sat
  - feature
  - safety
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/binary_sensor.py:200-389
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement advanced pressure health monitoring with EMA smoothing (alpha=0.05), linear regression drop-rate detection, 600s settle delay after boiler start, and 120s problem confirmation window. This goes beyond basic threshold monitoring to detect slow leaks and pressure decay patterns.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 EMA smoothing with alpha=0.05 applied to pressure readings
- [ ] #2 Linear regression calculates pressure drop rate over sliding window
- [ ] #3 600s settle delay after boiler start before analysis begins
- [ ] #4 120s confirmation window before declaring pressure problem
- [ ] #5 Pressure health binary sensor exposed via MQTT
<!-- AC:END -->
