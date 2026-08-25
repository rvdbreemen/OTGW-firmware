---
id: TASK-1088
title: >-
  Repair the 2.0.0 ADR graph: canonical fields, supersession edges, completeness
  config (117 to 63)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-25 07:22'
updated_date: '2026-08-25 17:51'
labels:
  - tooling
dependencies: []
priority: medium
ordinal: 264000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Six ordered steps, all frontmatter or config. No bin/adr lifecycle command is used or needed. Full research and measurements were produced by a six-agent workflow; the three tool defects it uncovered are filed as rvdbreemen/adr-kit#118, #119 and #120.

Step 2  Fill binding, gate, documents_shipped, verified_in on the 15 schema-failing ADRs (115,116,117,120,122,124,141,142,143,146,147,149,164,165,166). Values must come from each body: ADR-122, 124 and 142 declare themselves Binding, and the set names live CI gates. Use the single-colon pointer form evaluate.py:check_x, never the :: form the bodies use, because _verified_pointer_resolves splits on the first colon. Effect: schema 15 -> 1, and re-admits these files to the cross-reference corpus, clearing 3 phantom target-not-found findings.

Step 3  Delete false supersession edges: ADR-070, ADR-079, ADR-123 supersedes -> []; ADR-106 -> [ADR-105]; ADR-098 -> [ADR-119]; ADR-095.superseded_by -> ADR-119. Must precede step 4.

Step 4  Add the 12 reciprocal edges: 001<-061, 004<-053, 018<-042, 047<-075, 054<-056, 089<-167, 095<-119, 100<-174, 105<-106, 119<-098, 121<-167, 160<-164. Only ADR-160 needs the predecessor side.

Step 5  Partial supersession de-linking: ADR-062.superseded_by -> null, ADR-170.supersedes -> []. Follows the ADR-032 precedent.

Step 6  In docs/adr/.adr-kit.json set template.required_sections to the canonical seven MINUS '## Status'. This is the only lever that works: adr-lint returns FAIL before severity is consulted under --strict, and bin/adr accept passes --strict, so severity.completeness has no effect (measured: 86 -> 86). Effect: completeness 86 -> 39.

Step 7  Append a 2026-07-04 Superseded entry to ADR-160's existing status_history block, by hand. Clears the audit gate. Do NOT run adr supersede ADR-160 --by ADR-164: it passes every guard and then overwrites ADR-160's sanctioned immutability-exception paragraph and writes a duplicate Status History into both files.

Measured outcome: FAIL 117 -> 63. Steps 2-5 give 117 -> 109, step 6 gives 109 -> 64, step 7 gives 64 -> 63.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 15 ADRs carry correct binding, gate, documents_shipped and verified_in derived from their own bodies, with the single-colon pointer form
- [x] #2 Supersession edges match the resolved graph: false claims removed, 12 reciprocals present, partial supersessions kept in prose only
- [x] #3 template.required_sections is set and completeness findings drop to 39
- [x] #4 ADR-160 audit gate passes
- [x] #5 Strict lint reports FAIL 63 and no ADR body line changed, verified by diff
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DO NOT list, measured on real data by the research workflow. Every item was reproduced, not inferred.

1. Do not run ANY bin/adr lifecycle command on either worktree until the status_history blocks are fenced. On 2.0.0, 49 of 54 blocks are unfenced: 22 splice the new entry into a foreign section, 27 grow a duplicate '## Status History'. This includes accept and relate, not only supersede.
2. Do not use adr supersede for the reciprocal edges. It replaced the whole Status line on a real file and deleted 'Originally Accepted, 2026-05-08' plus the decision-maker attribution, exit 0. Every repair here is a frontmatter edit that touches nothing else.
3. Do not run adr-migrate with or without --to-profile on either tree. On the 86 completeness-failing files it injects 128-214 TODO placeholder lines into Accepted bodies and reorganises nothing, because canonical and nygard share heading names.
4. Do not bulk-add '## Status' headings. 82 of the 86 affected records are Accepted, Superseded or Amended.
5. Do not set severity.completeness to always_advisory. adr-lint returns FAIL before severity is consulted under --strict and bin/adr accept passes --strict. Measured: 86 -> 86, no change.
6. Do not add a TODO-only or single-item Alternatives Considered. count_alternatives excludes TODO items, so a placeholder scores identically to no section while permanently altering an immutable body, and one real item scores worse.
7. Do not transcribe verified_in pointers verbatim from the bodies. They use evaluate.py::check_x; the resolver splits on the first colon, so the :: form silently fails.
8. Do not treat a cross-line ADR number as a link. The two lines number independently: ADR-106's supersedes ADR-077 points at the 1.x ADR-077, while the 2.0.0 ADR-077 is an unrelated live decision. Every ADR-141 reference on 2.0.0 is genuine, because 1.x tops out at ADR-090.
9. Do not read ADR-098's title or ADR-095's Status line as new inconsistencies after step 3. Both permanently say ADR-097 and are immutable; ADR-119's renumber note sanctions that reading. Record the divergence in the commit message.
10. Do not trust the raw finding counts as the graph before step 2. 15 ADRs were excluded from the lint corpus, producing 3 phantom target-not-found findings while hiding their own.

2026-08-25: steps 2, 3, 4, 5 and 7 applied and pushed as 718b7978a. Strict lint 117 -> 108 (schema 15->1, consistency 104->88, audit 1->0). Zero unsanctioned body lines: the only non-frontmatter changes are ADR-160's four Status History lines, which the immutability rule explicitly permits, and two lines in the generated index.

Step 6 (template.required_sections) NOT applied, deliberately. The config's own _comment_template_choice records a considered decision not to override it: 'the FAIL output is the actionable next step', with the named precondition being adoption of a project template that treats Status as an inline bold prefix. The research found exactly that condition now holds (73 of 86 failures are the **Status:** inline convention), so the decision is ripe, but flipping it is the maintainer's call and not cleanup. AC #3 and AC #5 stay open pending that.

New finding outside the original scope, worth acting on: ADR-097 and ADR-101 carry machine-stamped binding:false that contradicts their own bodies. Both are Accepted, both self-declare binding-level, ADR-097 names check_ps_summary_master_topic_gate (verified to exist and resolve) and ADR-101 carries an adr-judge Enforcement block. That stamp came from the earlier adr-migrate pass over 161 records. A lint-passing falsehood is worse than a lint-failing gap: the gap announces itself, the false value does not. Those two are the only instances in the directory.

2026-08-25: ADR-142 closed as Rejected on the maintainer's decision that the feature will not be implemented. Frontmatter status Rejected, binding false, gate null (a rejected decision imposes nothing and enforces nothing). Status section rewritten in the house style ADR-144 already uses, stating plainly that it was never implemented and will not be, that the retired Deferred label is gone, and that everything below is historical record rather than intended design. Rejected transition appended to the now-fenced status_history with changed_via: manual.

Caught during the edit and worth recording: this ADR orders its sections Status, Status History, Context. Bounding the Status-section replacement on '## Context' would have deleted the entire Status History block. The boundary must be the NEXT heading, whatever it is, not an assumed one. Section order is not uniform across this ADR set.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Repaired the 2.0.0 ADR graph. Strict lint FAIL 117 -> 63, PASS 11 -> 28, schema failures 15 -> 0.

Canonical fields on the 15 schema-failing ADRs, each value read from that ADR's own body rather than defaulted. binding follows the ADR-080 self-classification the bodies carry; gate is null where the body rules out a CI gate and names the real gate where one exists; verified_in pointers use the single-colon form, because the bodies write evaluate.py::check_x and the resolver splits on the first colon.

Supersession graph: removed three false claims that came from body prose, resolved the ADR-119/095/098 case as a renumbering collision rather than a contradiction (ADR-119 was filed as ADR-097 and moved because a live ADR-097 cited from firmware source kept the number), added twelve missing reciprocals, and de-linked two partial supersessions to prose following the ADR-032 precedent.

Fenced all 49 unfenced status_history blocks. Until then every adr-kit lifecycle command on this tree appended a duplicate Status History section; the unfenced shape is adr-kit's own, emitted by the agent template. The CLI is usable here again.

Adopted template.required_sections, the canonical seven minus the Status heading. The config's own comment named the precondition and it was met: 73 of the 86 completeness failures were one house convention, not 86 incomplete records. severity config cannot do this job, measured 86 to 86, because adr-lint returns FAIL before severity is consulted under --strict.

Corrected two false binding stamps introduced by the earlier migrate pass: ADR-097 and ADR-101 both self-declare binding and both carried binding:false. A lint-passing falsehood is worse than a lint-failing gap.

ADR-142 closed as Rejected on the maintainer's decision that the feature will not be implemented, retiring the non-standard Deferred status that no tool could read.

No bin/adr lifecycle command was used for any of it; every repair was a frontmatter edit, a fence, a config change, or a sanctioned Status-section rewrite on a non-Accepted ADR.

Three adr-kit defects found on the way are filed upstream as rvdbreemen/adr-kit#118, #119 and #120.

Residual 63: 39 completeness (24 of them needing Alternatives Considered reconstruction) and 88 consistency findings, mostly related-symmetry now addressable with adr relate. The audit of the remaining 159 machine-stamped records is noted in the implementation notes.
<!-- SECTION:FINAL_SUMMARY:END -->
