---
id: TASK-218
title: 'SAT fix: set_temperature does not auto-map to preset'
status: To Do
assignee: []
created_date: '2026-04-09 05:26'
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
- [ ] #1 In satHandleTargetTemp, compare incoming temperature against all preset values
- [ ] #2 If temperature matches a preset, call satHandlePreset with the matched preset name instead
- [ ] #3 Only fall through to direct temperature set if no preset matches
- [ ] #4 Verify preset sync still fires correctly
<!-- AC:END -->
