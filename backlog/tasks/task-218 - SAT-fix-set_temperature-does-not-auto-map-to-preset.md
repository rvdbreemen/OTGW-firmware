---
id: TASK-218
title: 'SAT fix: set_temperature does not auto-map to preset'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:26'
updated_date: '2026-04-09 06:14'
labels:
  - audit-fix
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python climate.py:562-568: when set_temperature is called, it first checks if the temperature matches any configured preset value. If yes, it calls async_set_preset_mode instead. C++ satHandleTargetTemp only sets fTargetTemp directly without any preset matching. This means e.g. sending sat/target=21.0 when 21.0 is the eco preset will NOT activate eco preset in C++, but WOULD in Python SAT.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In satHandleTargetTemp, compare incoming temperature against all preset values
- [x] #2 If temperature matches a preset, call satHandlePreset with the matched preset name instead
- [x] #3 Only fall through to direct temperature set if no preset matches
- [x] #4 Verify preset sync still fires correctly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. In satHandleTargetTemp(), after parsing temperature, compare against all 6 preset values (comfort, eco, away, sleep, activity, home)
2. If a match is found (within float epsilon), call satHandlePreset() with the matching preset name and return true
3. Only proceed to updateSetting(SATtargettemp) if no preset matched
4. Verify preset sync still fires (it does since satHandlePreset handles that)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satHandleTargetTemp() now auto-maps incoming temperature to a preset when the value matches a configured preset within 0.05C (fabsf comparison). Uses a local struct array covering all 6 presets (comfort/eco/away/sleep/activity/home) and delegates to satHandlePreset() on match. Mirrors Python climate.py:562-568. No direct updateSetting() call when a preset matches, so preset sync and PID integral reset all fire correctly via the existing preset handler.
<!-- SECTION:FINAL_SUMMARY:END -->
