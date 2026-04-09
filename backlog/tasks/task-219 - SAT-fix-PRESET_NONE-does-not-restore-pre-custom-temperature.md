---
id: TASK-219
title: 'SAT fix: PRESET_NONE does not restore pre-custom temperature'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:27'
updated_date: '2026-04-09 06:14'
labels:
  - audit-fix
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python climate.py:614-616: when preset_mode is set to PRESET_NONE, it restores the previously saved _pre_custom_temperature as the target temperature. C++ satHandlePreset with 'none' only resets fPreCustomTemp to 0.0f but does NOT restore the saved temperature to fTargetTemp. The user's previous custom temperature is lost when clearing a preset in C++.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In satHandlePreset, when PRESET_NONE, restore fTargetTemp from fPreCustomTemp if non-zero
- [x] #2 Reset fPreCustomTemp to 0.0f after restoration
- [x] #3 Publish updated target temp to MQTT after restoration
- [x] #4 Verify behavior matches Python: preset->none restores previous custom temperature
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. In satHandlePreset(), when newPreset == SAT_PRESET_NONE: check if fPreCustomTemp > 0.0f
2. If yes, restore settings.sat.fTargetTemp = state.sat.fPreCustomTemp, then reset fPreCustomTemp = 0.0f
3. Publish the restored temperature to sat/target via sendMQTTData (using dtostrf + valBuf pattern)
4. If fPreCustomTemp is 0.0f, just leave fTargetTemp as-is (no previous custom temp was saved)
5. Also reset PID integral to avoid overshoot on temp jump
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satHandlePreset() PRESET_NONE branch now restores settings.sat.fTargetTemp from state.sat.fPreCustomTemp when it is non-zero, resets fPidI to avoid overshoot, and immediately publishes the restored temperature to sat/target via sendMQTTData. fPreCustomTemp is cleared after restoration. When fPreCustomTemp is 0 (no prior preset was active), behaviour is unchanged. Mirrors Python climate.py:614-616.
<!-- SECTION:FINAL_SUMMARY:END -->
