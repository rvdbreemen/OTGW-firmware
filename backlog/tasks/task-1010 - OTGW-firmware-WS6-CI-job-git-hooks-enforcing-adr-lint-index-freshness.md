---
id: TASK-1010
title: 'OTGW-firmware WS6: CI job + git hooks enforcing adr lint + index freshness'
status: To Do
assignee: []
created_date: '2026-07-04 16:31'
labels:
  - adr-kit
  - governance
  - ci
dependencies: []
ordinal: 222000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Turn 'someone remembers' into 'the build fails'. GitHub Actions adr-governance job on PRs touching docs/adr/**: adr lint --strict + adr index --check. Pre-commit hook in .githooks/ mirrors it (same pattern as prerelease/commit-msg hooks). Optional thin check_adr_index_fresh in evaluate.py. Enforcement lands after WS1 migration so the baseline is clean. Full plan: docs/plan/adr-kit-governance-plan.md. Repo: OTGW-firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A PR that edits an ADR status without reindexing fails CI
- [ ] #2 A PR that adds an ADR without an index entry fails CI
- [ ] #3 Pre-commit hook runs adr lint + adr index --check on staged docs/adr changes
- [ ] #4 Enforcement enabled only after the WS1 migration baseline is clean
<!-- AC:END -->
