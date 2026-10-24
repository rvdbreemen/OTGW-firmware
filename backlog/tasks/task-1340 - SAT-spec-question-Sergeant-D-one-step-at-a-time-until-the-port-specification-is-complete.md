---
id: TASK-1340
title: >-
  SAT spec: question Sergeant D one step at a time until the port specification
  is complete
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T09:19:00Z'
updated_date: '2026-10-24T09:25:00Z'
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
<!-- SECTION:NOTES:END -->
