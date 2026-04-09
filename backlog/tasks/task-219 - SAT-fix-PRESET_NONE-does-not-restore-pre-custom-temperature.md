---
id: TASK-219
title: 'SAT fix: PRESET_NONE does not restore pre-custom temperature'
status: To Do
assignee: []
created_date: '2026-04-09 05:27'
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
- [ ] #1 In satHandlePreset, when PRESET_NONE, restore fTargetTemp from fPreCustomTemp if non-zero
- [ ] #2 Reset fPreCustomTemp to 0.0f after restoration
- [ ] #3 Publish updated target temp to MQTT after restoration
- [ ] #4 Verify behavior matches Python: preset->none restores previous custom temperature
<!-- AC:END -->
