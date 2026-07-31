---
id: TASK-1008
title: >-
  adr-kit WS4: lifecycle commands (propose/accept/supersede/reject)
  mutate+reciprocate+reindex
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
updated_date: '2026-07-06 19:54'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 220000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Humans should never hand-edit a status line, a reciprocal link, or the index. Each command mutates frontmatter + reciprocals + Status History then runs adr index: propose, accept [--by], supersede <old> --by <new> (stamps both files), reject --reason (terminal, kept as trail). Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr supersede 160 --by 164 reproduces the two-file edit + index update done by hand this session
- [ ] #2 No lifecycle command leaves the index stale (adr index --check green afterward)
- [ ] #3 Each command appends a Status History entry
<!-- AC:END -->

## Comments

<!-- COMMENTS:BEGIN -->
author: Codex
created: 2026-07-06 19:54
---
Moved to adr-kit backlog as TASK-20; original OTGW task archived because the work belongs to the adr-kit repo board.
---
<!-- COMMENTS:END -->
