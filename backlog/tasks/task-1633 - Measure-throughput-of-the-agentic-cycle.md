---
id: TASK-1633
title: Measure throughput of the agentic cycle
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:44:00Z'
updated_date: '2026-11-05T04:30:00Z'
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
Report from repo data only: task lead time (created -> Done) from backlog frontmatter, time from question asked to entry recorded from open-questions.md / the TASK-1630 index, age of open questions, number of tasks labelled needs-maintainer-ok, alpha tags per day from git log.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A script (scripts/) prints these figures for a chosen date range
- [x] #2 One run over the last 30 days is stored in docs/process/ with the date
- [x] #3 The notes name the largest waiting time and where it sits
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. scripts/agentic_throughput.py (stdlib) reports tasks (created, Done, lead time median/p80, open stock, needs-maintainer-ok), SAT questions (asked-to-answer time from the UTC time in the queue row and the UTC time on the entry's Source line; pairs without both times or with a negative wait are excluded and counted), and prerelease tags per day from git log. First run showed a -15.5 h wait: date-only and local-time rows paired wrongly, so only UTC-timed, non-negative pairs count now (12 measured, 19 excluded). Run over 2026-10-06..2026-11-04 on the full local clone stored as docs/process/agentic-throughput-2026-11-05.md. Largest waiting time: the open stock (76 To Do, 21 In Progress, mostly waiting on hardware, field runs or decisions), not Sergeant D (median 0.3 h, p80 2.4 h, longest 6.5 h) and not the build/bench path (8.7 prerelease tags per day). Decision latency of Robert is not timestamped and not measured.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Throughput is measurable from the repository. AC1: scripts/agentic_throughput.py prints task lead times, SAT question answer times, open questions with age, needs-maintainer-ok count and prerelease tags per day for a chosen window. AC2: the 30-day run is stored in docs/process/agentic-throughput-2026-11-05.md. AC3: the largest waiting time sits in the open task stock (76 To Do / 21 In Progress); Sergeant D answers within 2.4 h for 80% of the timed questions and the build/bench path produced 8.7 tags per day. Limits stated in the script header and the report (lead time from updated_date; 19 of 31 answers not timeable; Robert's decision latency not measured).
<!-- SECTION:FINAL_SUMMARY:END -->
