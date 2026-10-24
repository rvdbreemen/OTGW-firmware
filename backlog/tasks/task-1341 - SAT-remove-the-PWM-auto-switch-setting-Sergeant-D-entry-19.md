---
id: TASK-1341
title: 'SAT: remove the PWM auto switch setting (Sergeant D entry 19)'
status: To Do
assignee: []
created_date: '2026-10-24T09:24:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 464000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 08:47 UTC: 'remove it' (entry 19). Remove settings.sat.bPwmAutoSwitch end to end like Force PWM in TASK-1332: SATtypes.h field, settingStuff read/write (old files with the key load silently), restAPI GET/POST + settings list, MQTT command and HA switch entity if present (retire the retained discovery config), v2.js field, settingsHelp, docs; the sustained-overshoot switch (satCycleCheckAutoSwitch) always runs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No code path reads a PWM auto switch setting; the sustained-overshoot switch always runs; host test red on HEAD (setting off blocks the switch) and green
- [ ] #2 Setting/REST/MQTT/HA/UI field gone; an old settings file with the key loads; a retained HA config (if one existed) is cleared
<!-- AC:END -->
