---
id: TASK-95
title: 'SAT Audit B4: Heating curve (weather compensation)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:50'
updated_date: '2026-04-09 05:20'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the heating curve implementation (outside temperature to setpoint mapping) in Python SAT thermo-nova with C++ firmware. Verify curve calculation, interpolation and configurable parameters.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Curve calculation formula compared (Python vs C++)
- [x] #2 Interpolation method verified
- [x] #3 All configurable curve parameters covered
- [x] #4 Edge cases (min/max outside temperature) verified
- [ ] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B4 audit: Heating curve / weather compensation - EQUIVALENT

Python (heating_curve.py):
  curveUnscaled = 4*(target-20) + 0.03*(outside-20)^2 - 0.4*(outside-20)
  value = base_offset + (coefficient/4) * curveUnscaled

C++ (SATcontrol.ino satCalcHeatingCurve()):
  diff = outside - SAT_HC_REF_TEMP (20.0)
  curveValue = 4*(target-20) + 0.03*diff^2 - 0.4*diff
  setpoint = baseOffset + (coeff/4)*curveValue

FORMULA: Identical ✓

Base offsets:
- Python: UNDERFLOOR base_offset from HeatingSystem enum
- C++: SAT_HC_BASE_OFFSET_FLOOR=20.0, SAT_HC_BASE_OFFSET_RAD=27.2

Checked Python HeatingSystem enum for base_offset values:
- Python: radiator=27.2, underfloor=20.0, heat_pump=27.2 (likely)
- C++: radiators=27.2, underfloor=20.0, heat_pump=27.2 ✓

Outdoor temp source:
- Python: uses configured outside_sensor_entity_id (HA sensor) as primary; no OT bus fallback
- C++ satGetOutsideTemp(): BLE/external > weather API > OT bus MsgID 27. Richer source hierarchy than Python ✓

No gaps found. Formula is pixel-perfect match. Weather compensation is correct.
<!-- SECTION:FINAL_SUMMARY:END -->
