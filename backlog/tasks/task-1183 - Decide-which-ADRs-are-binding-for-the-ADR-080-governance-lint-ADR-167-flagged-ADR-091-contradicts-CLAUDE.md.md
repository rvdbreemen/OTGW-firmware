---
id: TASK-1183
title: >-
  Decide which ADRs are binding for the ADR-080 governance lint (ADR-167
  flagged, ADR-091 contradicts CLAUDE.md)
status: To Do
assignee: []
created_date: '2026-09-30 11:35'
updated_date: '2026-09-30 13:10'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision input, measured 2026-09-30 (case-sensitive parse of the frontmatter; the earlier 143-flip count was a regex error with re.I):
- Option A, frontmatter 'binding:' is authoritative: 6 ADRs carry binding: true (097, 101, 146, 177 Accepted, all with their gates present; 122 and 124 Superseded). The live lint passes. But all four ADRs CLAUDE.md lists as binding carry binding: false (004, 088, 091, 167), although 004, 088 and 091 do have CI gates in evaluate.py. Their frontmatter looks defaulted at the adr-kit migration; correcting it means editing Accepted ADRs, which the immutability rule forbids without the maintainer.
- Option B, the Status section: needs a precise parser; not measured.
- Option C, CLAUDE.md's list: the lint keeps failing on ADR-167 until 167 leaves that list; CLAUDE.md itself says 167 has no CI gate, so under ADR-080 it is not a pattern-level binding ADR.
Smallest consistent end state, for the maintainer to confirm: frontmatter authoritative in scripts/adr_governance.py; binding: true set for 004, 088 and 091 (an authorised metadata correction, or an amending ADR if the immutability rule covers frontmatter); ADR-167 moved to the structural list in CLAUDE.md.
<!-- SECTION:NOTES:END -->
