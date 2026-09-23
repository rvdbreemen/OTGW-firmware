---
id: TASK-1159
title: Re-enabling SAT through the settings endpoint does not clear a safety trip
status: To Do
assignee: []
created_date: '2026-09-23 18:15'
labels:
  - sat
  - bug
dependencies: []
priority: medium
ordinal: 291000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
satHandleEnabled() (REST /sat/enable, MQTT enabled) clears state.sat.bSafetyTripped; updateSetting(SATenabled) does not. A trip leaves settings.sat.bEnabled true, so a settings POST of true is a no-op and a false->true toggle via settings also leaves the trip set (observed 2026-09-23 on the bench: disable+enable via /api/v2/settings, safety_tripped stayed true).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Whatever path the web UIs use to resume SAT after a trip actually clears the trip
- [ ] #2 The trip-clear logic lives in one place and is not triggered by the boot-time settings file parse
- [ ] #3 Bench: trip, resume through the UI path, safety_tripped becomes false
<!-- AC:END -->
