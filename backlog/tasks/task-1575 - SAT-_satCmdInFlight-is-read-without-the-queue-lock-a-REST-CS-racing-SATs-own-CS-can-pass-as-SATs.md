---
id: TASK-1575
title: >-
  SAT: _satCmdInFlight is read without the queue lock, a REST CS= racing SAT's
  own CS= can pass as SAT's
status: To Do
assignee: []
created_date: '2026-11-02T21:47:00Z'
labels:
  - sat
dependencies: []
priority: medium
ordinal: 719000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Peer review finding 6 (queue lane, 2026-11-02, pre-existing): addCommandToQueue reads the plain global _satCmdInFlight (SATcontrol.ino, TASK-1150) before taking the TASK-1569 queue lock. A REST/MQTT CS= arriving on async_tcp while the loop task has the flag set for SAT's own CS= is treated as SAT's own and not refused (ADR-179, TASK-1236). Fix direction: pass the ownership explicitly (an addCommandToQueue parameter or a SAT-only entry point) instead of a cross-task global.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test or harness shows an external CS= enqueued while SAT's flag is set is refused as external
- [ ] #2 SAT's own CS= still accepted; OTDirect handler unchanged in behaviour
- [ ] #3 Full tests folder green, evaluate --quick 0 failed
<!-- AC:END -->
