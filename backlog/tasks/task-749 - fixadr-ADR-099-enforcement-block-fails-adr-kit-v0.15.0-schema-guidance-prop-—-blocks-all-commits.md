---
id: TASK-749
title: >-
  fix(adr): ADR-099 enforcement block fails adr-kit v0.15.0 schema (guidance
  prop) — blocks all commits
status: Done
assignee: []
created_date: '2026-05-29 08:32'
updated_date: '2026-05-29 09:49'
labels: []
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
bug-045: docs/adr/ADR-099-ha-discovery-friendly-name-format.md Enforcement block has a 'guidance' property (~line 94) not allowed by adr-kit v0.15.0 schema (only forbid_pattern/forbid_import/require_pattern + llm_judge). bin/adr-judge crashes exit 2 on EVERY commit via .githooks/pre-commit, blocking all commits. Workaround in use: ADR_KIT_HOOK_DISABLE=1. Relocate the LLM-judge guidance into a schema-valid home without changing the ADR-099 decision (Accepted; tooling metadata repair only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 adr-judge exits 0 on a no-op staged diff with ADR check enabled (no bypass)
- [ ] #2 ADR-099 enforcement block validates against adr-kit v0.15.0 schema
- [ ] #3 LLM-judge guidance intent for the friendly-name count check is preserved
- [ ] #4 pre-commit no longer needs ADR_KIT_HOOK_DISABLE for ordinary commits
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Superseded by TASK-755 (parallel session migrated ADR-099 enforcement block to adr-kit 0.15.0 schema; commit 798e7d35). ADR check should now pass without ADR_KIT_HOOK_DISABLE.
<!-- SECTION:NOTES:END -->
