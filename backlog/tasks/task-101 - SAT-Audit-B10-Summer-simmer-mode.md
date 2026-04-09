---
id: TASK-101
title: 'SAT Audit B10: Summer simmer mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:51'
updated_date: '2026-04-09 05:23'
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
Compare the summer simmer mode in Python SAT thermo-nova with C++ firmware. Verify activation logic (outside temperature threshold), minimum setpoint in summer mode and deactivation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Activation logic and thresholds compared
- [x] #2 Minimum setpoint behavior in summer simmer verified
- [x] #3 Deactivation logic verified
- [x] #4 Interaction with heating curve verified
- [x] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read Python summer_simmer.py and C++ satUpdateSummerSimmer()
2. Compare activation thresholds, setpoint behavior during summer, deactivation, heating curve interaction
3. Document gaps and create fix tasks
4. Check off ACs
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Summer Simmer Index formula: C++ matches Python exactly (summer_simmer.py:31-38). Perception thresholds identical.

2. Activation logic: C++ uses accumulator (hours above threshold) + 2C hysteresis on deactivation. Python SAT does NOT have this outdoor-temp-based summer-mode activation; it only uses the SummerSimmer INDEX as comfort-adjusted room temp. The C++ summer mode feature (bSummerSimmer) is a firmware-side extension not from Python SAT.

3. Setpoint in summer mode: C++ sends CS=10 + CH=0. Python COLD_SETPOINT=22 is used for demand detection (flow>COLD_SETPOINT), not as CH setpoint directly. No behavioral gap for the C++ approach.

4. THERMAL COMFORT GAP: Python climate.py:276-277 uses SummerSimmer.index as room temp when thermal_comfort is enabled. C++ does not implement this. Fix task TASK-204 created.

5. Heating curve interaction: C++ summer mode returns early before heating curve calculation - correct.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B10: Summer Simmer Mode

The Summer Simmer Index formula in C++ matches Python summer_simmer.py exactly, including the F<58 bypass and all perception thresholds.

Key finding: C++ implements a distinct outdoor-temperature-based summer mode (bSummerSimmer) with accumulator logic and 2C deactivation hysteresis. This is a firmware-side feature not present in Python SAT. Python SAT only uses SummerSimmer as a thermal comfort substitute for room temperature in the PID loop.

Gap: Python thermal_comfort mode (SummerSimmer.index substituted as PID room temp input) is missing in C++. Fix task TASK-204 created.

No critical setpoint behavior differences found.
<!-- SECTION:FINAL_SUMMARY:END -->
