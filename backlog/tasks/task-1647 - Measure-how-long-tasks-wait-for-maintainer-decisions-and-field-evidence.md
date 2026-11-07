---
id: TASK-1647
title: Measure how long tasks wait for maintainer decisions and field evidence
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T04:33:00Z'
updated_date: '2026-11-05T17:27:00Z'
labels:
  - process
  - agentic
milestone: 2.1.0
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
- [x] #1 A documented, repeatable way to date when a task starts and stops waiting for a maintainer decision or for field evidence
- [x] #2 scripts/agentic_throughput.py reports both waits for a chosen window, with its limits stated
- [x] #3 One run stored in docs/process/ and the largest wait named in the task notes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Signal: git history of task files (no new tooling). Maintainer wait = label needs-maintainer-ok (legacy needs-decision); field wait = status In Review or label needs-field-data (legacy waiting-for-field, field-verification). Clock = updated_date in the same diff, else commit time. Done ends every wait. Extend scripts/agentic_throughput.py, host tests with mutants, convention in docs/guides/backlog-cli.md, one stored run in docs/process/.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-05 (session, maintainer decision): parked until after 2.0.0.

Robert 2026-11-05 (session, /goal): park lifted, implement now.

Largest wait (30-day run, 2026-10-07..2026-11-05): field evidence. TASK-1151 has waited 44.3 d (In Review since 2026-09-22, then To Do + waiting-for-field on 2026-11-04, one continuous wait). Longest ended maintainer wait 13.5 d (TASK-1217, traced by hand: labelled at creation 4918d4b99 2026-10-11 23:24Z, Done 2026-10-25 10:37Z). Maintainer waits: 20 ended, median 0.4 d, p80 5.3 d; 3 open, oldest 7.1 d (TASK-1473). Field waits: only 2 ended (median 10.5 d, longest 18.7 d TASK-1071); 2 open. Small sample because markers were used unevenly; the convention now makes them measurable.

Cross-check (second source): open waits from git history vs markers in current task files matched 12/12; 7 of those were stale markers on Done tasks, which led to the rule that Done ends a wait; after it, 5/5 non-Done tasks match. Runtime 17 s.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: convention 'Marking a wait' in docs/guides/backlog-cli.md (needs-maintainer-ok; In Review or needs-field-data; Done ends a wait); start/stop dated from task-file git history, clock = updated_date in the diff. AC2: scripts/agentic_throughput.py reports both waits (ended in window: count, median, p80, longest; open now: count, oldest), limits in the header; tests/test_agentic_throughput_waits.py 7/7 green, red on two mutants (no netting: 2 fail; no Done stop: 1 fails). Cross-check git history vs task files 5/5 open waits. AC3: docs/process/agentic-waits-2026-11-05.md; largest wait = field evidence, TASK-1151 44.3 d (notes). Gates: evaluate.py --quick 83 pass / 0 fail; full suite 154/162, the 8 failures reference none of the changed files (test_ota_flash passes standalone; test_ot_support_migration fails standalone on firmware code this task did not touch). Not verified: nothing on hardware (none needed).
<!-- SECTION:FINAL_SUMMARY:END -->
