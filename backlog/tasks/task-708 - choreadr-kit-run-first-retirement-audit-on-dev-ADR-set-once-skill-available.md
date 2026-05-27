---
id: TASK-708
title: 'chore(adr-kit): run first retirement-audit on dev ADR set once skill available'
status: To Do
assignee: []
created_date: '2026-05-26 11:01'
labels:
  - adr-kit
  - tooling
  - blocked
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Blocked on rvdbreemen/adr-kit issue #8 landing.\n\nOnce /adr-kit:retirement-audit is available, run it on the dev worktree (79 ADRs, ADR-001 to ADR-080). ADR-078 (defer HA Core aliases to 2.0.0) is already a likely candidate on dev since the SAT removal makes the defer-decision moot. Review all candidates and archive or deprecate as appropriate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 /adr-kit:retirement-audit skill is available in installed adr-kit version
- [ ] #2 Audit run on dev worktree; all candidates reviewed with user
- [ ] #3 ADR-078 specifically evaluated: deprecate or supersede as appropriate
- [ ] #4 Same audit run on 2.0.0 worktree (111 ADRs)
<!-- AC:END -->
