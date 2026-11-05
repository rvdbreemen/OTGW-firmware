---
id: TASK-1647
title: Measure how long tasks wait for maintainer decisions and field evidence
status: To Do
assignee: []
created_date: '2026-11-05T04:33:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: low
ordinal: 771000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1636 (Robert 2026-11-05: no scaling now). docs/process/agentic-throughput-2026-11-05.md shows the wait sits in the open task stock (76 To Do, 21 In Progress), not in Sergeant D or the build/bench path. What the stock waits on is not measured: there is no timestamp for 'waiting for Robert's decision' or 'waiting for field evidence'. Find a cheap, honest signal (e.g. task notes or labels that mark the blocker with a date, or the needs-maintainer-ok label history in git) and extend scripts/agentic_throughput.py with it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A documented, repeatable way to date when a task starts and stops waiting for a maintainer decision or for field evidence
- [ ] #2 scripts/agentic_throughput.py reports both waits for a chosen window, with its limits stated
- [ ] #3 One run stored in docs/process/ and the largest wait named in the task notes
<!-- AC:END -->
