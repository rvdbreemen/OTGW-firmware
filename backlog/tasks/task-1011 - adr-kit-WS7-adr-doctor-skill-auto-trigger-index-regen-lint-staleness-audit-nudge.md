---
id: TASK-1011
title: >-
  adr-kit WS7: 'adr doctor' + skill auto-trigger (index regen, lint,
  staleness/audit nudge)
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 223000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Whenever ADR skills run, check whether the index needs regenerating and whether an audit is warranted, and act. adr doctor = lint + index --check + staleness: (a) Proposed whose verified_in resolves (shipped-but-unaccepted), (b) Proposed older than N days, (c) Accepted whose verified_in files changed since acceptance (code-drift, e.g. ADR-160/147), (d) named-but-absent ADR-080 gates. Skill/adr-generator runs doctor at START and index+lint at END; material drift auto-triggers an audit. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit + skills.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr doctor reports the four staleness classes plus index/lint status
- [ ] #2 ADR skill/adr-generator runs doctor at start and index+lint at end, auto-fixing the index
- [ ] #3 A shipped-but-Proposed ADR is surfaced as a fast-path candidate
- [ ] #4 Material drift auto-triggers an audit pass
<!-- AC:END -->
