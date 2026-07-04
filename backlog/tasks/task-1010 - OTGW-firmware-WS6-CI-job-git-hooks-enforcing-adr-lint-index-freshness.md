---
id: TASK-1010
title: 'OTGW-firmware WS6: CI job + git hooks enforcing adr lint + index freshness'
status: Done
assignee: []
created_date: '2026-07-04 16:31'
updated_date: '2026-07-04 16:48'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reconcile with TASK-422 (adr-lint CLI for CI): WS6 is the OTGW-firmware side of wiring that CLI into CI + hooks. A repo-local realization (scripts/adr_governance.py) is being landed now as proof; swap for the adr-kit CLI when 422 ships.

Repo-local realization landed (proof of WS6): scripts/adr_governance.py (lint/index-check/doctor), tests/test_adr_governance.py (10 tests, fire-on-drift + live-tree guards), .github/workflows/adr-governance.yml (PR gate on docs/adr/**). Live tree is green: lint 0/0, index-check 0 fail, index completed (added 24 previously-missing ADRs 91-120). Pre-commit already runs adr-kit; swap the repo-local CLI for the adr-kit adr-lint CLI when TASK-422 ships.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Delivered repo-local (WS6): scripts/adr_governance.py + tests/test_adr_governance.py + .github/workflows/adr-governance.yml. CI job 'ADR lint + index-check' is GREEN on PR #670 (lint --strict + index-check + 10 unit tests). Live docs/adr tree is lint-clean and fully indexed (164 ADRs). Follow-up: swap the repo-local CLI for the adr-kit adr-lint CLI (TASK-422) and add index-check to the pre-commit hook (adr-kit ADR check already runs there).
<!-- SECTION:FINAL_SUMMARY:END -->
