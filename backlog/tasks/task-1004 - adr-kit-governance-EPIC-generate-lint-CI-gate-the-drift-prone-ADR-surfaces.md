---
id: TASK-1004
title: >-
  adr-kit governance (EPIC): generate + lint + CI-gate the drift-prone ADR
  surfaces
status: To Do
assignee: []
created_date: '2026-07-04 16:30'
updated_date: '2026-07-04 16:46'
labels:
  - adr-kit
  - governance
  - epic
dependencies: []
ordinal: 216000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Umbrella for the adr-kit governance improvements from the 2026-07-04 ADR audit. Principle: derive the drift-prone surfaces (status lines, supersession links, README index, gate compliance) instead of hand-maintaining them. Full plan: docs/plan/adr-kit-governance-plan.md. Tracks WS1 schema+migrate, WS2 lint, WS3 index, WS4 lifecycle commands, WS5 after-the-fact fast path, WS6 CI+hooks, WS7 skill auto-trigger + doctor. Phased: A foundation (WS1/2 warn-only) -> B generation (WS3/4) -> C flow (WS5) -> D enforcement (WS6 blocking) -> E autonomy (WS7).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reconciliation: adr-kit already has overlapping roadmap tasks (TASK-417..424, 749). WS1~TASK-424, WS2/WS6~TASK-422. Net-new from this audit: WS3 index generator, WS4 atomic lifecycle commands, WS5 documents-shipped auto-accept, WS7 doctor+skill-hook. Repo-local proof of WS2/WS3/WS6 landed via scripts/adr_governance.py (OTGW-firmware).

Proof landed for WS2/WS3/WS6 (repo-local): scripts/adr_governance.py + CI workflow + unit tests; live docs/adr tree now lint-clean and fully indexed (164 ADRs, 0 missing, 0 dup). The governance tool found and closed 24 index gaps beyond the audit window (ADR-91..120). WS1/WS4/WS5/WS7 remain adr-kit-repo work (reconcile with TASK-422/424).
<!-- SECTION:NOTES:END -->
