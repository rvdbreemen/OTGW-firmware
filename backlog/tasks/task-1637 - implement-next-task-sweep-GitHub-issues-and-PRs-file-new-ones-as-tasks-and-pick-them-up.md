---
id: TASK-1637
title: >-
  implement-next-task: sweep GitHub issues and PRs, file new ones as tasks and
  pick them up
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T23:46:00Z'
updated_date: '2026-11-04T23:54:00Z'
labels:
  - process
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 764000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05: the implement-next-task cycle must also look at GitHub issues and pull requests; a new issue or PR becomes a backlog task and is picked up.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SKILL.md step 0 has a GitHub sweep (open issues and PRs, dedupe against existing tasks by GitHub number)
- [x] #2 New bug issues and PRs get a task and go first in step 1 selection; merge of another person's PR still needs Robert's go
- [x] #3 A dry run of the sweep commands on the live repo lists the open issues/PRs and which already have a task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: added Step 0b to .claude/skills/implement-next-task/SKILL.md (gh issue/pr list, URL dedupe over backlog/tasks+completed+archive, file with URL in description, bugs/PRs high and first in step 1, features/proposals needs-maintainer-ok + ask Robert, PR review in own worktree, merge only on Robert's go per PR) and a step 1 selection bullet. Dry run on the live repo: 4 open issues (#694, #692, #154, #10), 0 open PRs; URL dedupe 0 hits each; #N grep showed #693->TASK-1608, #695->TASK-1609 (known) and false positives for #10, which is why the URL is the dedupe key. Filed TASK-1639 (#694), 1640 (#692), 1641 (#154), 1642 (#10), all proposals/features -> needs-maintainer-ok.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
implement-next-task now sweeps GitHub issues and PRs every cycle (Step 0b) and files new ones as tasks with the URL as dedupe key; bug reports and PRs are picked up first, feature requests wait for Robert. Evidence: dry run listed 4 issues / 0 PRs, URL dedupe 0 hits, 4 tasks filed (1639-1642). Not verified: a PR flow end to end (no open PR now).
<!-- SECTION:FINAL_SUMMARY:END -->
