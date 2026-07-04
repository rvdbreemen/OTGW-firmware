---
id: TASK-1004
title: >-
  adr-kit governance (EPIC): generate + lint + CI-gate the drift-prone ADR
  surfaces
status: To Do
assignee: []
created_date: '2026-07-04 16:30'
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
