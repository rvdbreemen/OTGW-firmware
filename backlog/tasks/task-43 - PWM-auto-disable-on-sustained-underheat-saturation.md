---
id: TASK-43
title: PWM auto-disable on sustained underheat/saturation
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 21:05'
updated_date: '2026-04-05 22:53'
labels:
  - sat
  - feature
dependencies:
  - TASK-14
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
- [x] #1 Auto-switch PWM to continuous when underheat persists for 180s
- [x] #2 Auto-switch PWM to continuous when saturation (100% duty) persists for 300s
- [x] #3 Event/log emitted when auto-switch occurs
- [ ] #4 Underheat and saturation conditions clearly defined and documented
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Auto-switch PWM->continuous when boiler temp <= setpoint-0.3C sustained for 180s.
<!-- SECTION:FINAL_SUMMARY:END -->
