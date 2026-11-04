---
id: TASK-103
title: 'SAT Audit B12: Valve opening protection'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:51'
updated_date: '2026-04-09 05:25'
labels:
  - audit
  - sat
  - phase-2
milestone: m-0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare the valve opening protection in Python SAT thermo-nova with the C++ firmware. Verify periodic valve exercise, schedule and behavior when valve has been closed too long.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Valve exercise interval and duration compared
- [x] #2 Detection of valve closed too long verified
- [x] #3 Action during valve exercise (temporary setpoint increase) verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Review Python valve detection logic in climate.py:415-455
2. Review Python window sensor logic in climate.py:800-828
3. Review C++ bValvesOpen handling and window detection
4. Document findings and gaps
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Valve exercise: Neither Python SAT nor C++ firmware has a periodic valve exercise feature. The valve protection in Python is simply "stop heating when all valves closed" (valves_open property). C++ mirrors this via bValvesOpen state. The task description assumed valve exercise exists - it does not in SAT.

2. Valve open detection: Python detects valves_open from TRV climate entity states (hvac_action=HEATING or temp < target + offset). C++ receives bValvesOpen externally via MQTT topic sat/valves_open. Architecturally different but appropriate - C++ cant query HA entities directly. When no radiators configured, Python returns True (no detection possible); C++ equivalent: bValvesOpen defaults to true.

3. Window sensor timing: Python default 15s (window_minimum_open_time_seconds). C++ default 60s (iWindowMinOpenSec). Different defaults but both user-configurable. No functional gap.

4. Action on window: Python switches to Activity preset. C++ does same: saves current preset, switches to SAT_PRESET_ACTIVITY.

No fix tasks needed - differences are architectural or configuration-level.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B12: Valve opening protection

Neither Python SAT nor C++ has a periodic valve exercise feature. The "valve protection" in both is simply suppressing heating when all valves are detected as closed.

Python detects valves_open by querying TRV climate entity states; C++ receives this via MQTT (sat/valves_open) from HA. Both architectures are correct for their environment.

Window sensor action matches: both switch to the Activity preset when window is open for the minimum time. Default timeout differs (Python 15s vs C++ 60s) but both are configurable. No fix tasks required.
<!-- SECTION:FINAL_SUMMARY:END -->
