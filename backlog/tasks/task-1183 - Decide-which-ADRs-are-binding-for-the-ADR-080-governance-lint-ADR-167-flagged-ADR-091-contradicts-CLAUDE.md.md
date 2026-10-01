---
id: TASK-1183
title: >-
  Decide which ADRs are binding for the ADR-080 governance lint (ADR-167
  flagged, ADR-091 contradicts CLAUDE.md)
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 11:35'
updated_date: '2026-10-01 04:36'
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
- [x] #1 The maintainer names the authoritative source for 'binding' (frontmatter, the Status section, or CLAUDE.md)
- [x] #2 scripts/adr_governance.py follows that source, and tests/test_adr_governance.py passes on the live tree
- [x] #3 CLAUDE.md's binding ADR list and the ADR frontmatter agree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision input, measured 2026-09-30 (case-sensitive parse of the frontmatter; the earlier 143-flip count was a regex error with re.I):
- Option A, frontmatter 'binding:' is authoritative: 6 ADRs carry binding: true (097, 101, 146, 177 Accepted, all with their gates present; 122 and 124 Superseded). The live lint passes. But all four ADRs CLAUDE.md lists as binding carry binding: false (004, 088, 091, 167), although 004, 088 and 091 do have CI gates in evaluate.py. Their frontmatter looks defaulted at the adr-kit migration; correcting it means editing Accepted ADRs, which the immutability rule forbids without the maintainer.
- Option B, the Status section: needs a precise parser; not measured.
- Option C, CLAUDE.md's list: the lint keeps failing on ADR-167 until 167 leaves that list; CLAUDE.md itself says 167 has no CI gate, so under ADR-080 it is not a pattern-level binding ADR.
Smallest consistent end state, for the maintainer to confirm: frontmatter authoritative in scripts/adr_governance.py; binding: true set for 004, 088 and 091 (an authorised metadata correction, or an amending ADR if the immutability rule covers frontmatter); ADR-167 moved to the structural list in CLAUDE.md.

Maintainer decision (Robert, 2026-09-30, asked in session): frontmatter is the authoritative source for 'binding'. Authorised: set binding: true on ADR-004, ADR-088 and ADR-091 as a metadata correction of Accepted ADRs, and move ADR-167 to the structural list in CLAUDE.md (it has no CI gate, so under ADR-080 it is not pattern-level binding).

Implemented: scripts/adr_governance.py takes 'binding' from the frontmatter (binding: true); an ADR without frontmatter is not binding. ADR-088 and ADR-091 frontmatter corrected to binding: true with their evaluate.py gates (authorised metadata correction); docs/adr/ADR-INDEX.json regenerated with adr-kit's adr-index (only those two entries changed). ADR-004 is Superseded (by ADR-053, which does not restate the String rule), so per the follow-up decision it leaves the binding list and keeps binding: false; TASK-1187 gives the rule an Accepted home. CLAUDE.md and AGENTS.md carry one identical ADR-list block (maintainer, 2026-10-01): binding 088, 091, 097, 101, 146, 177; ADR-167 moved to the structural list; AGENTS.md's stale ADR-089 entry is gone. Also fixed in the script: unclosed file handles, and os.path.relpath() failing on Windows for a path on another drive.
Evidence: old vs new script on the live tree: the HEAD heuristic marks [62, 81, 91, 129, 130, 146, 167] binding and the lint FAILs on ADR-167; the frontmatter rule marks [88, 91, 97, 101, 146, 177] and the lint gives 0 fail (1 old WARN on ADR-62). python tests/test_adr_governance.py: 15 tests OK, including three new parse tests (frontmatter true is binding; the word 'Binding' alone is not; no frontmatter is not) and a live test that the CLAUDE.md binding list equals the frontmatter set and that AGENTS.md carries the identical block. Before the change the same file gave 2 failures and 3 errors. Host suite: 34/34.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The ADR governance lint now reads 'binding' from the ADR frontmatter, as the maintainer decided, and the live lint is clean (it failed on ADR-167 because the old text heuristic took the title of ADR-080 for a binding rule). ADR-088 and ADR-091 carry binding: true with their gates; ADR-004 (Superseded) left the binding list, follow-up TASK-1187. CLAUDE.md and AGENTS.md share one identical ADR-list block, and tests/test_adr_governance.py enforces both the list-to-frontmatter match and the identical block.
Evidence per AC: AC#1 the decision is in the notes (frontmatter, 2026-09-30, and the ADR-004 follow-up decision). AC#2 the script follows the frontmatter; tests/test_adr_governance.py 15/15 OK on the live tree (was 2 failures and 3 errors with the old script), live lint 0 fail. AC#3 test_instruction_lists_match_frontmatter passes: CLAUDE.md's list [88, 91, 97, 101, 146, 177] equals the Accepted binding: true set, and AGENTS.md's block is identical. Host suite 34/34. No firmware change, so no version bump or build.
<!-- SECTION:FINAL_SUMMARY:END -->
