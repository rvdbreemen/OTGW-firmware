---
id: TASK-43
title: PWM auto-disable on sustained underheat/saturation
status: To Do
assignee: []
created_date: '2026-04-05 21:05'
labels:
  - sat
  - feature
dependencies: []
references:
  - >-
    other-projects/SAT-releases-thermo-nova/custom_components/sat/heating_control.py:307-378
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Automatically switch from PWM back to continuous modulation when underheat persists for 180s or saturation (100% duty) persists for 300s. Prevents inefficient PWM operation when the boiler cannot keep up.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Auto-switch PWM to continuous when underheat persists for 180s
- [ ] #2 Auto-switch PWM to continuous when saturation (100% duty) persists for 300s
- [ ] #3 Event/log emitted when auto-switch occurs
- [ ] #4 Underheat and saturation conditions clearly defined and documented
<!-- AC:END -->
