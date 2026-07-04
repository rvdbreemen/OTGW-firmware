---
id: TASK-1009
title: >-
  adr-kit WS5: after-the-fact ADR fast path (documents-shipped ->
  evidence-backed auto-accept)
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
dependencies: []
ordinal: 221000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
5 ADRs sat in Proposed for weeks though each documented already-shipped code. adr document scaffolds documents_shipped:true and requires a verified_in pointer. adr accept auto-accepts iff documents_shipped AND every verified_in resolves AND lint passes AND quality>=threshold, recording evidence in Status History. Guardrails: only documents_shipped eligible; forward-looking ADRs keep the human checkpoint; dangling pointer blocks; auto vs assist(confirm) strictness configurable. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: adr-kit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr document scaffolds documents_shipped:true and requires a verified_in pointer
- [ ] #2 documents_shipped + resolving evidence + clean lint auto-accepts with an audit-trail Status History entry
- [ ] #3 Broken pointer or documents_shipped:false does NOT auto-accept
- [ ] #4 auto vs assist strictness configurable
<!-- AC:END -->
