---
id: TASK-2902
title: >-
  Derive the SAT_EVENT record list for the cloud recording from the firmware's
  SAT code
status: To Do
assignee: []
created_date: '2026-11-15T12:19:00Z'
updated_date: '2026-11-15T12:21:00Z'
labels:
  - cloud-recording
  - sat
dependencies: []
priority: low
ordinal: 1004000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2901 decision 3 (Robert, 2026-11-15). The cloud recording is telemetry of the firmware, not of thermo-nova (Robert, 2026-11-15: 'Het is namelijk telemetrie van de firmware en niet van Thermonova'). Derive which state transitions and decision points of our SAT port become SAT_EVENT records from the firmware code itself (SATcontrol.ino, SATcycles.ino, SATpid.ino, SATpressure.ino), with file:line per event. No Sergeant D question needed: this describes what the port does, not what SAT should do. A difference with thermo-nova found along the way is a separate SAT question under .claude/rules/sat-ground-truth.md, outside this task. Low priority, on hold with TASK-1473.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT_EVENT list with per event the trigger, payload fields and firmware file:line
- [ ] #2 Byte estimate per event and rate, checked against the no-PSRAM budget
<!-- AC:END -->
