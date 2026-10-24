---
id: TASK-1340
title: >-
  SAT spec: question Sergeant D one step at a time until the port specification
  is complete
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T09:19:00Z'
updated_date: '2026-10-24T21:45:00Z'
labels:
  - sat
  - spec
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 463000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer directive 2026-10-24: unclear SAT behaviour is resolved interactively with Sergeant D in the #alpha-testing chat, one question per message with the possible answers and our advice; each answer is recorded in docs/sat/sergeant-d-corrections.md and becomes a backlog task. Queue and statuses: docs/sat/open-questions.md (English, readable by him). Sources for questions: the TASK-1331 review (Q1-Q4) and the SAT port audit of 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every question in docs/sat/open-questions.md is answered (recorded as a corrections entry) or dropped with a reason
- [ ] #2 Every answered question has a backlog task that implements it, linked from the queue
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24: Q1 (PWM auto switch) asked in the chat with answers A/B and advice A (msg 1563481923404169270).

2026-10-24: Q1 answered by Sergeant D 08:47 UTC ('remove it') -> entry 19, TASK-1341. Q2 (per-sample control setpoint for the end-of-cycle switch) asked.

2026-10-24: cadence per Robert: check every 4 h; after 12 h unanswered with 10+ other messages since, repeat once with a link. Loop cron moved to every 4 h.

2026-10-24: cadence tightened by Robert: check hourly (silent), repeat after 12 h when 3+ other messages since. Cloud routine feasibility checked: see session.

2026-10-24: cloud routine declined by Robert (bot token would have to live in the cloud environment). Loop stays local: hourly session cron.

2026-10-24: SAT port audit done (docs/sat/sat-port-audit-2026-10-24.md, 69 findings: 9 high, 27 medium, 33 low, none critical). 16 of his statements recorded as entries 20-35 (quotes verified by script, 115/116 exact, 1 elided). Queue extended with Q5-Q18. High findings without a question: TASK-1344 (MM release), TASK-1345 (fallback), TASK-1346 (OT-Direct CH), TASK-1347 (wizard coefficient).

2026-10-24 21:14-21:18 UTC: Sergeant D answered Q2,Q3,Q6,Q8,Q9,Q10,Q11,Q13 -> entries 36-38,40-44 (second-read), OT-loss rule entry 39; tasks 1364-1370; ADR-197 (Q10) and ADR-199 (Q13) answers recorded. Q4 and Q12 asked again with detail, Q7 clarification asked.
<!-- SECTION:NOTES:END -->
