---
id: TASK-1487
title: >-
  SAT presets and window target survive a reboot as thermo-nova (TASK-1466
  decision 7)
status: To Do
assignee: []
created_date: '2026-10-29T20:32:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 646000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: as thermo-nova climate.py:169-190 (restore preset_mode, pre-custom temperature, target). Today satHandlePreset/_satCheckWindowTimer write the preset target into settings.sat.fTargetTemp in RAM only (SATcontrol.ino:1394, :1562) while eActivePreset and fPreCustomTemp are state; a later settings flush can persist the preset target as the user's own target. Persist active preset + pre-preset temperature (+ window pre-target) and keep the user's own target separate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a reboot the active preset is still active and preset 'none' restores the user's own temperature (host test red on HEAD); an unrelated settings save never stores a preset temperature as the user's target
<!-- AC:END -->
