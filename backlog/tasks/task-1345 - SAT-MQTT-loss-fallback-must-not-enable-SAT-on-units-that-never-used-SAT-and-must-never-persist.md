---
id: TASK-1345
title: >-
  SAT: MQTT-loss fallback must not enable SAT on units that never used SAT, and
  must never persist
status: To Do
assignee: []
created_date: '2026-10-24T10:11:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 468000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #2 (verified 2026-10-24 by reading SATcontrol.ino:4706-4719): with settings.mqtt.bEnable and a past connection, a >5 min broker outage sets settings.sat.bEnabled=true on ANY unit, also one that never used SAT; a settings save in that window persists it. Needs a decision (Sergeant D / Robert): which units may fall back, and the fallback must live in state, not settings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Fallback never writes settings.sat.bEnabled (state flag only) - host test red on HEAD (save during outage persists) and green
<!-- AC:END -->
