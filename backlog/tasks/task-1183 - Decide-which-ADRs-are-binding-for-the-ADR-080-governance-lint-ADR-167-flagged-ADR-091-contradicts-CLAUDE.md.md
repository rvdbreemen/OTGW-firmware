---
id: TASK-1183
title: >-
  Decide which ADRs are binding for the ADR-080 governance lint (ADR-167
  flagged, ADR-091 contradicts CLAUDE.md)
status: To Do
assignee: []
created_date: '2026-09-30 11:35'
labels:
  - adr
  - governance
dependencies: []
priority: low
ordinal: 306000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Surfaced by TASK-1182. With the ADR-INDEX.md crash fixed, tests/test_adr_governance.py runs its live-tree lint and fails on one item:
'ADR-167: binding+Accepted names gate(s) [check_heap_fragmentation_promotion, check_heap_tier_entry_counters, check_heap_tier_thresholds_ordered, check_per_consumer_heap_gate] absent from evaluate.py/tests'.
Cause: scripts/adr_governance.py decides 'binding' with a full-text heuristic (the word 'Binding' anywhere, unless 'guideline-level' appears) and collects every check_* token as a named gate. ADR-167's only 'Binding' is the title of ADR-080 in its Related section, and it names the four gates only as removed. Its frontmatter says binding: false, gate: null.
The sources disagree more widely: the frontmatter binding field and the text heuristic differ for 11 ADRs (062, 081, 091, 095, 097, 101, 129, 130, 142, 167, 177). CLAUDE.md lists ADR-091 and ADR-167 as binding, while their frontmatter says binding: false, and CLAUDE.md itself says ADR-167 has no CI gate.
Switching the lint to frontmatter-first would silently stop gate-checking ADR-091 (CLAUDE.md: binding, gated by check_design_system_drift). This is a maintainer decision about which source is authoritative, not a code defect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The maintainer names the authoritative source for 'binding' (frontmatter, the Status section, or CLAUDE.md)
- [ ] #2 scripts/adr_governance.py follows that source, and tests/test_adr_governance.py passes on the live tree
- [ ] #3 CLAUDE.md's binding ADR list and the ADR frontmatter agree
<!-- AC:END -->
