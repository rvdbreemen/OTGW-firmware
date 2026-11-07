---
id: TASK-1638
title: 'ADR lint: explicit superseded_by null in frontmatter wins over status prose'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T23:49:00Z'
updated_date: '2026-11-04T23:51:00Z'
labels:
  - process
  - agentic
  - adr
dependencies: []
priority: high
ordinal: 764000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by TASK-1626: the first push-triggered ADR governance run on dev failed. lint --strict turns the one WARN 'ADR-62: has superseded_by but status is Accepted' into exit 1. ADR-062 says superseded_by: null in its frontmatter and is only partly superseded (ADR-170 replaces its automatic readback; the rest stays in force), but scripts/adr_governance.py fell back to the status prose ('superseded by ADR-170') whenever the frontmatter list was empty, also when the key was present with null. TASK-1183 makes the frontmatter the source of truth. Fix: fall back to prose only when the superseded_by key is absent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 lint --strict on dev exits 0 (was 1, ADR-62 WARN)
- [x] #2 A unit test in tests/test_adr_governance.py fails on the old parser and passes on the fix: frontmatter superseded_by: null plus prose 'superseded by ADR-N' is not superseded
- [x] #3 No other ADR changes its lint result (0 fail, 0 warn over 210 ADRs; index-check unchanged)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. Reproduced on a clean origin/dev worktree: python scripts/adr_governance.py lint --strict -> 'lint: 0 fail, 1 warn over 210 ADRs', exit 1 (WARN ADR-62). Fix in parse_adr: fall back to the status prose only when the superseded_by key is absent from the frontmatter. Effect over all ADRs: lint 0 fail 0 warn (only ADR-62 changes), index-check 0 fail. Test: SupersededByParseTests (2 tests) in tests/test_adr_governance.py; on the old parser FAILED (failures=1: test_explicit_null_superseded_by_beats_status_prose), on the fix 17 tests OK. First draft of the test class was named ParseTests and was silently shadowed by an existing class of that name further down the file (15 tests ran); renamed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
lint --strict is green again on dev. The parser now treats an explicit superseded_by: null in the frontmatter as authoritative and uses the status prose only when the key is absent. AC1: lint 0 fail 0 warn, exit 0 (was 1 on origin/dev). AC2: new test red on the old parser, green on the fix. AC3: no other ADR changes its result; index-check unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
