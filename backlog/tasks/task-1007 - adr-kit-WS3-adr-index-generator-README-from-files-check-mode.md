---
id: TASK-1007
title: 'adr-kit WS3: ''adr index'' generator (README from files, --check mode)'
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 219000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The hand-maintained index rotted (19 missing, 12 stale labels, a dup, wrong counts). Generate entries/status markers/supersession notes/per-topic counts from the ADR files. 'adr index --check' exits non-zero on drift (CI/pre-commit assertion). Human prose preserved via sentinels; tool owns only generated tables. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr index reproduces a correct README: zero missing ADRs, no duplicate, correct counts
- [ ] #2 adr index --check green immediately after generation; idempotent
- [ ] #3 Human narrative sections preserved via sentinels
<!-- AC:END -->
