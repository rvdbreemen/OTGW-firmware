---
id: TASK-1417
title: >-
  SAT PID: fixed deadband 0.1, remove the deadband setting and its widening
  (entry 76, Q26)
status: To Do
assignee: []
created_date: '2026-10-28T06:45:00Z'
updated_date: '2026-10-28T19:14:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
references:
  - docs/sat/sergeant-d-corrections.md
priority: medium
ordinal: 581000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 76, Q26: fixed 0.1). Deadband constant 0.1 as thermo-nova const.py:16; remove SATdeadband setting, REST/MQTT/UI exposure and the widening during room-temperature estimation. Sites listed in entry 76 (SATtypes.h, settingStuff.ino, settingsHelp.h, MQTTstuff.ino, restAPI.ino, SATcontrol.ino, SATpid.ino, v2.js, handleDebug.ino). API change: update docs/api/MQTT.md and REST docs. Update ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test: deadband is 0.1 regardless of a stored SATdeadband value and while the room temperature is estimated
- [ ] #2 Setting, REST field, MQTT set command and UI control removed; docs/api updated
- [ ] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency replaced by TASK-1368 (own host-test AC#1; PID tasks run one at a time after 1368).
<!-- SECTION:NOTES:END -->
