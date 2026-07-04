---
id: TASK-1005
title: 'adr-kit WS1: canonical frontmatter schema + ''adr migrate'' for prose-only ADRs'
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 217000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Root enabler: 35/43 ADRs are prose-only so nothing is machine-checkable. Define the frontmatter schema (adds binding:bool, gate:str|null, documents_shipped:bool, verified_in:[file:symbol|commit]) and ship 'adr migrate' to back-fill frontmatter from legacy prose Status blocks, idempotent, body bytes unchanged. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Documented schema incl. binding/gate/documents_shipped/verified_in
- [ ] #2 adr migrate back-fills every prose-only ADR, idempotent, body unchanged (frontmatter-only diff)
- [ ] #3 Shared schema-validation function reused by lint
<!-- AC:END -->
