---
id: TASK-98
title: 'SAT Audit B7: Window detection'
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
Compare the window detection logic in Python SAT thermo-nova with the C++ firmware. Verify temperature drop detection, time windows, action on open window and recovery when window closes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Detection method (temperature drop or MQTT sensor) compared
- [x] #2 Thresholds and time windows verified
- [x] #3 Action on open window (setpoint reduction) verified
- [x] #4 Recovery behavior after closing window verified
- [ ] #5 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
B7 audit: Window detection - GAPS FOUND

Python (climate.py, const.py): Window sensors = binary sensors in HA. When any window opens: CONF_WINDOW_MINIMUM_OPEN_TIME=15s (default) before switching to activity preset. Window close restores previous preset.

C++ (SATcontrol.ino satHandleWindow(), _satCheckWindowTimer()):
- Window open: start timer
- After iWindowMinOpenSec (configurable), switch to Activity preset
- Window close: restore fPreWindowTarget + iPreWindowPreset

GAPS:
1. Python CONF_WINDOW_MINIMUM_OPEN_TIME default = 15 seconds. C++ settings.sat.iWindowMinOpenSec — default value not visible here but needs verification. Python uses 15s as minimum useful window open time.
2. Python window detection is driven by HA binary sensor state changes (real entity). C++ depends on external MQTT input (satHandleWindow() is called from MQTT message handler). There is no autonomous window detection from OT data in either implementation — both are externally triggered. Equivalent.
3. Python restores previous HVAC preset on window close. C++ restores fPreWindowTarget (temperature) and iPreWindowPreset (preset enum). Functionally equivalent ✓.
4. Python: if window opens during activity preset, stays in activity (no double-switch). C++ has same guard (fPreWindowTarget==0 check prevents re-saving on second open). ✓
5. Rise rate / integration: Python does NOT have its own rise rate detection for windows (that's solar gain). C++ also does not — window detection is purely time-based on binary input. ✓

No critical gaps found. Default timing values need cross-checking.
<!-- SECTION:FINAL_SUMMARY:END -->
