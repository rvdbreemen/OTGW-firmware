---
id: TASK-104
title: 'SAT Audit B13: Override and manual mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:51'
updated_date: '2026-04-09 05:27'
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
Compare the override/manual mode in Python SAT thermo-nova with C++ firmware. Verify activation, persistence, fallback to automatic and interaction with presets.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Override activation and deactivation compared
- [x] #2 Persistence of manual mode verified
- [x] #3 Fallback to automatic mode verified
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read Python async_set_temperature, async_set_preset_mode, async_set_hvac_mode
2. Read C++ satHandleTargetTemp, satHandlePreset
3. Compare behavior for override, preset management, persistence and fallback
4. Document gaps, create fix tasks
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit complete. Findings:

1. Override mechanism: Both Python and C++ use the same concept (target temp + preset). No dedicated "manual override" mode in Python SAT; PRESET_NONE is the manual state.

2. Auto preset mapping: Python async_set_temperature maps to preset if temp matches preset value. C++ satHandleTargetTemp never checks preset values. GAP - fix task TASK-218 created.

3. PRESET_NONE restore: Python restores _pre_custom_temperature when clearing preset. C++ only clears fPreCustomTemp, does NOT restore target temp. GAP - fix task TASK-219 created.

4. Persistence: C++ routes all target temp changes through updateSetting() which persists to flash. Python does not persist state to flash (HA handles it). C++ is more persistent.

5. Fallback to automatic: Neither Python nor C++ has a time-based automatic fallback from manual/preset mode. Both remain in preset until explicitly changed.

6. hvac_mode (heat/off): Python supports HVAC mode switching. C++ has SAT enabled/disabled but no explicit heat/off HVAC mode. Functionally equivalent.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## SAT Audit B13: Override and manual mode

Two behavioral gaps found:

1. TASK-218: C++ satHandleTargetTemp does not auto-map temperature values to presets (Python does this in async_set_temperature).

2. TASK-219: C++ satHandlePreset with PRESET_NONE does not restore the pre-custom temperature (Python does in async_set_preset_mode).

Persistence differs architecturally (C++ flushes to flash, Python relies on HA state storage) but both behave correctly for their platform.

No time-based automatic fallback exists in either implementation.
<!-- SECTION:FINAL_SUMMARY:END -->
