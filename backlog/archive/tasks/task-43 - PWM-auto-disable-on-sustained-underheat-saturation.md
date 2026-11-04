---
id: TASK-43
title: PWM auto-disable on sustained underheat/saturation
status: Done
assignee:
  - '@claude'
created_date: 2026-04-05 21:05
updated_date: '2026-10-29T19:16:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-1412.1 records review (R38): 2026-10-29. current-status annotation (R38/LL-023, reviewed at 3baed73d8; history kept). Final summary 'boiler temp <= setpoint-0.3C' and description 'saturation (100% duty)' are superseded:
- Underheat: flow <= requested - 3.0 for 180 s from the first qualifying tick (SATcycles.ino:30-31, :1084-1093; entry 86 Q44 A).
- Saturation: last PWM off time == 0 for 300 s (:32, :1096-1104).
- Both reset outside PWM ON and during or 300 s after DHW (:1077-1081; entry 108, our interpretation).
AC#4 (definitions documented) is still unchecked; the definitions now live in the SATcycles.ino:1067-1074 comment and the entries above. Open follow-ups: TASK-1428, TASK-1412.8. Dependency on TASK-14 is ambiguous until the ID repair (see TASK-1412.1). Done is no physical-performance certificate.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Auto-switch PWM->continuous when boiler temp <= setpoint-0.3C sustained for 180s.
<!-- SECTION:FINAL_SUMMARY:END -->
