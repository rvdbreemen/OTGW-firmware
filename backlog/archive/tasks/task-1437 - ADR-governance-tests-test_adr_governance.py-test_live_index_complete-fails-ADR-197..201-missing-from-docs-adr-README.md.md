---
id: TASK-1437
title: >-
  ADR governance: tests/test_adr_governance.py test_live_index_complete fails,
  ADR-197..201 missing from docs/adr/README.md
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T21:49:00Z'
updated_date: '2026-10-29T06:08:00Z'
labels:
  - adr
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 598000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-10-28 after accepting ADR-198: 'ADR-197 (Proposed) / ADR-198 (Accepted) / ADR-199 (Proposed) / ADR-200 (Accepted) / ADR-201 (Accepted) is MISSING from docs/adr/README.md' although README mentions them (format the checker expects differs). Not verified whether it already failed on the commit before b6dd96366 (ADR-200/201 untouched by that commit suggests yes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cause identified (README format vs gov.readme_entries); test green; red-before evidence noted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Cause: gov.readme_entries() (scripts/adr_governance.py:134-141) counts only markdown links of the form ](ADR-NNN-...md) in docs/adr/README.md. adr-index maintains only the generated table, which lists bare filenames (no links), so it cannot fix this: adr-index docs/adr --check and a write run both report 'changed: False' on b07e3194a. ADR-197..201 were accepted without a curated link entry in the topic sections. Red before (b07e3194a): python tests/test_adr_governance.py -> Ran 15, FAILED (failures=1), test_live_index_complete: ADR-197/198/199/200/201 (Accepted) MISSING. Fix: one curated entry each, placed after the ADR it supersedes (200 after 022, 201 after 164, 197 after 069, 198 after 071, 199 after 195); stale markers corrected (022/069/071/164/195 Superseded with 'Superseded by ADR-NNN', 196 Proposed->Accepted). Green after: Ran 15, OK; adr-index --check still changed: False.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
docs/adr/README.md: curated index entries for ADR-197..201 added, stale status markers fixed. Root cause: the governance checker counts markdown links, adr-index only regenerates the bare-filename table, so newly accepted ADRs need a curated entry by hand. Evidence: tests/test_adr_governance.py red on b07e3194a (1 failure, 5 ADRs missing), green after (15 OK); adr-index --check changed: False before and after. Docs-only, no firmware path, no bump. Not done: a gate that tells the author to add the curated entry at accept time (the test already catches it after the fact).
<!-- SECTION:FINAL_SUMMARY:END -->
