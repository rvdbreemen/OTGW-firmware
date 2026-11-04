---
id: TASK-1620
title: Measure throughput of the agentic cycle
status: To Do
assignee: []
created_date: '2026-11-04 21:50'
updated_date: '2026-11-04 21:51'
labels:
  - process
  - agentic
dependencies: []
priority: low
ordinal: 760000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F6. The cycle is deliberately serialised (one build machine-wide, bench test per task, at most 3 h between builds). Whether the bottleneck is Robert, Sergeant D or the bench is not measured.
Report from repo data only: task lead time (created -> Done) from backlog frontmatter, time from question asked to entry recorded from open-questions.md / the TASK-1617 index, age of open questions, number of tasks labelled needs-maintainer-ok, alpha tags per day from git log.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1611.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A script (scripts/) prints these figures for a chosen date range
- [ ] #2 One run over the last 30 days is stored in docs/process/ with the date
- [ ] #3 The notes name the largest waiting time and where it sits
<!-- AC:END -->
