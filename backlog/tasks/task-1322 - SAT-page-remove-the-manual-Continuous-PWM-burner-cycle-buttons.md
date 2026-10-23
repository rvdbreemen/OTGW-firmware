---
id: TASK-1322
title: 'SAT page: remove the manual Continuous/PWM burner cycle buttons'
status: To Do
assignee: []
created_date: '2026-10-23T15:43:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 445000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-23 15:08Z (#alpha-testing msg 1563207530275152037): 'the continuous and pwm buttons should never be present in the SAT dashboard. Because the software decides when every mode will be in charge.' TASK-1310 moved the Burner cycle mode card to the Control view; this removes the manual mode buttons. Check what they write today (POST /api/v2/sat/mode, satSetControlMode, settings.sat.bForcePWM?) and whether a read-only indicator of the active mode should stay (the mode is still useful to see). Decide with Sergeant D whether the force-PWM setting itself stays (setting vs dashboard button).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The SAT page has no buttons that set Continuous/PWM; the active mode is still shown read-only (screenshot)
- [ ] #2 No other UI path changes the mode; the REST endpoint decision (keep for API users or remove) recorded with Sergeant D's answer
<!-- AC:END -->
