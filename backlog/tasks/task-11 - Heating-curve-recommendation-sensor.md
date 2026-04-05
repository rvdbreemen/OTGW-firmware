---
id: TASK-11
title: Heating curve recommendation sensor
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:06'
updated_date: '2026-04-05 23:27'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python calculates a recommendation for the heating curve coefficient based on average error over daily samples. If mean error is consistently positive (room too cold), recommend INCREASE. Consistently negative (too warm): DECREASE. Within deadband*2: HOLD. Too few samples (<6): INSUFFICIENT_DATA. This helps users optimize their curve coefficient without manual experimentation. On the ESP we already have the error available in state.sat.fError. We need to maintain a rolling average.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Rolling error buffer: array of 24 samples (1 per hour, or every N control cycles)
- [x] #2 Calculation: mean_error over buffer, compare with 2*deadband threshold
- [x] #3 State enum SATCurveRecommendation: INCREASE, DECREASE, HOLD, INSUFFICIENT_DATA
- [x] #4 State field: state.sat.eCurveRecommendation
- [x] #5 State field: state.sat.fMeanError (average error over sample window)
- [x] #6 REST API: GET /api/v2/sat/status includes curve_recommendation and mean_error fields
- [x] #7 MQTT publish: sat/curve_recommendation (string: increase/decrease/hold/insufficient)
- [ ] #8 WebUI: recommendation visible in Heating Curve section with color code
- [x] #9 HA auto-discovery: sensor entity for curve_recommendation in mqttha.cfg
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT thermo-nova algorithmic reference (heating_curve.py:10-40):
- Quadratic formula: curve_value = 4*(T_target - 20) + 0.03*(T_outside - 20)^2 - 0.4*(T_outside - 20)
- Final setpoint = base_offset + (coefficient / 4) * curve_value
- base_offset and coefficient are user-configurable parameters
- The quadratic term (0.03) compensates for increased heat loss at very low outdoor temps
- The linear term (-0.4) provides baseline correction
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heating curve recommendation sensor.\n\nChanges:\n- 24-sample ring buffer (1 per hour) tracks error history\n- Mean error compared with 2*deadband threshold\n- Recommendation: increase/decrease/hold/insufficient\n- REST: curve_recommendation, mean_error fields\n- MQTT: sat/curve_recommendation (string)\n- HA auto-discovery: sensor entity\n- AC#8 (WebUI color coding) left for WebUI polish phase\n\nFiles: OTGW-firmware.h, SATcontrol.ino, mqttha.cfg
<!-- SECTION:FINAL_SUMMARY:END -->
