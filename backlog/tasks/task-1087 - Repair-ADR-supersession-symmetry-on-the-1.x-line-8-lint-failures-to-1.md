---
id: TASK-1087
title: Repair ADR supersession symmetry on the 1.x line (8 lint failures to 1)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-25 07:21'
updated_date: '2026-08-25 09:09'
labels:
  - tooling
dependencies: []
priority: medium
ordinal: 187000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seven Accepted ADRs name a successor that does not name them back, plus one mis-diagnosis. All fixable by frontmatter edit alone; no body change, no CLI.

Edits:
  ADR-053.supersedes += ADR-004    ADR-042.supersedes += ADR-018
  ADR-061.supersedes += ADR-047    ADR-056.supersedes += ADR-054
  ADR-088.supersedes += ADR-073    ADR-078.supersedes += ADR-077
  ADR-032.superseded_by: ADR-056 -> null

ADR-032 is not a missing reciprocal: it is Accepted and its own Status line says 'Partially superseded by ADR-056 ... ADR-032 remains the baseline'. Clearing the machine field matches the 2.0.0 tree's ADR-032, which is the house precedent for partial supersession staying in prose.

Measured on a scratchpad copy: strict lint FAIL 8 -> 1. The residual is ADR-084 missing a Related Decisions section, which is a body edit to an Accepted ADR and is out of scope.

DO NOT use bin/adr supersede for this. Proven on real data in this repo: it replaces the whole Status line and deleted 'Originally Accepted, 2026-05-08' plus the decision-maker attribution from ADR-073, exit 0. Filed upstream as rvdbreemen/adr-kit#120.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The seven reciprocal supersedes entries are present and strict lint reports FAIL 1 on the 1.x tree
- [x] #2 ADR-032 superseded_by is null and its prose is unchanged
- [x] #3 No ADR body line changed, verified by diff
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Repaired supersession symmetry on the 1.x ADR set by frontmatter edit only. Strict lint FAIL 8 -> 1, PASS 13 -> 19.

Six successors gained the reciprocal supersedes entry: ADR-053<-004, ADR-042<-018, ADR-061<-047, ADR-056<-054, ADR-088<-073, ADR-078<-077.

ADR-032 was a mis-diagnosis rather than a missing reciprocal. It is Accepted and its own Status line reads 'Partially superseded by ADR-056 for protected admin endpoints ... ADR-032 remains the baseline for unauthenticated local-network interfaces outside that boundary.' A partial supersession is not a lifecycle transition, so the machine field was cleared to null and the prose left untouched. That matches the 2.0.0 tree's own ADR-032, which already carries superseded_by: null, so the two lines now agree.

Deliberately did NOT use bin/adr supersede. Reproduced on real data during the research: it replaces the entire Status line and silently drops content recorded only there, exit 0. Filed upstream as rvdbreemen/adr-kit#120.

Verified: zero body lines changed across all seven files. Residual failure is ADR-084 missing a Related Decisions section, which is a body edit to an Accepted ADR and is out of scope by the immutability rule.
<!-- SECTION:FINAL_SUMMARY:END -->
