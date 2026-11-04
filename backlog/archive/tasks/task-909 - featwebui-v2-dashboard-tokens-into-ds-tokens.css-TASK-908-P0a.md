---
id: TASK-909
title: 'feat(webui): v2 dashboard tokens into ds-tokens.css (TASK-908 P0a)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 04:31'
updated_date: '2026-06-24 04:39'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 125000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Fold mockup tokens (--hot/--cold/--dhw/--gauge-track + zone/tile aliases) into ds-tokens.css per Patterns reconciliation. No layout change. Plan: docs/superpowers/plans/2026-06-24-webui-v2-coexisting-redesign.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New tokens in light+dark; zone/tile map to status palette; evaluate --quick green incl drift gate
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Folded v2 dashboard tokens into ds-tokens.css: 4 new physical tokens (--hot/--cold/--dhw/--gauge-track) + --gauge-text alias to --fg-1, and zone-*/tile-* aliased to the official status palette (ADR-091 reconciliation). Light+dark. evaluate --quick green, drift gate green.
<!-- SECTION:FINAL_SUMMARY:END -->
