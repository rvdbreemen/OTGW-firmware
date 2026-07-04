---
id: TASK-1006
title: 'adr-kit WS2: ''adr lint'' validation engine (status/link/gate consistency)'
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 218000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace human review of ADR consistency; make ADR-080 machine-checked. Rules: schema-valid frontmatter; superseded_by non-empty <=> status Superseded; supersedes/superseded_by reciprocity; binding+Accepted implies named gate exists in evaluate.py/tests or ADR is guideline-level; quality-gate scoring; verified_in resolves. --strict for CI + JSON output. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Non-zero exit on missing frontmatter, status/link contradiction, broken reciprocity, or accepted-binding-ADR with absent gate
- [ ] #2 ADR-080 gate rule resolves the named gate against the consuming repo
- [ ] #3 --strict (CI) and JSON output modes
<!-- AC:END -->
