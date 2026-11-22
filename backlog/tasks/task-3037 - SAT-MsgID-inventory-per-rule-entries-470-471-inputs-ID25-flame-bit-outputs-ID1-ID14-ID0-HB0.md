---
id: TASK-3037
title: >-
  SAT: MsgID inventory per rule (entries 470/471: inputs ID25 + flame bit;
  outputs ID1, ID14, ID0 HB0)
status: To Do
assignee: []
created_date: '2026-11-22T16:44:00Z'
updated_date: '2026-11-22T16:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1145000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Entries 470/471 (private SAT register): SAT's inputs are ID25 (flow) and the flame bit, its outputs ID1 (control setpoint), ID14 (max modulation) and ID0 high byte bit 0 (CH enable). This task inventories every SAT control rule against that, so Sergeant D can decide per rule. No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A table of every SAT control rule with the MsgIDs it reads and writes, each with file:line on origin/dev
- [ ] #2 Each rule that reads anything beyond ID25 and the flame bit is flagged with its fallback (e.g. the ID28 return temperature, SATcontrol.ino:1300-1310; the ID17 users incl. satUpdateModulationReliability; the outside temperature)
- [ ] #3 The result is posted to Sergeant D as one page for his decisions
- [ ] #4 The inventory becomes a reference document of how SAT works, under docs/sat/: per rule its inputs, outputs, source entry and file:line (Robert, #dev-sat-mqtt 1574103281544859800: 'feed the knowledge on how it's done and validate. The implementation and the code with that.')
- [ ] #5 Per rule a host test that calls the real code and goes red when the rule is broken
- [ ] #6 Rules without an entry and entries without a rule are listed as findings
- [ ] #7 Sergeant D reviews the document
<!-- AC:END -->
