---
id: TASK-1575
title: >-
  SAT: _satCmdInFlight is read without the queue lock, a REST CS= racing SAT's
  own CS= can pass as SAT's
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T21:47:00Z'
updated_date: '2026-11-03T00:17:00Z'
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
- [x] #1 Host test or harness shows an external CS= enqueued while SAT's flag is set is refused as external
- [x] #2 SAT's own CS= still accepted; OTDirect handler unchanged in behaviour
- [x] #3 Full tests folder green, evaluate --quick 0 failed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03: the SAT origin of a CS= is now an argument (addCommandToQueue(..., satOwn), handleOTDirectCommand(..., satOwn)); the global _satCmdInFlight and satCommandInFlight() are removed. AC1 tests/test_pic_pm_chain.py test_external_cs_refused_while_sat_enqueues_its_own: old Core lets the external CS= through (ext=1), fix refuses it (ext=0) and SAT's own is queued (sat=1); 29/29. AC2 OTDirect handler: same rule, now on the argument (OTDirect.ino:2771); SAT harnesses green. AC3 full tests folder: first run showed 7 SAT harnesses with a 4-arg addCommandToQueue stub not compiling; stubs got a 5th default param; all 98 files green after. evaluate 0 failed.
<!-- SECTION:NOTES:END -->
