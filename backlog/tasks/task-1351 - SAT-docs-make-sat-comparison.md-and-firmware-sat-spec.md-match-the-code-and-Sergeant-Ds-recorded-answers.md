---
id: TASK-1351
title: >-
  SAT docs: make sat-comparison.md and firmware-sat-spec.md match the code and
  Sergeant D's recorded answers
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T16:27:00Z'
updated_date: '2026-10-24T17:00:00Z'
labels:
  - sat
  - docs
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 486000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #36 (audit IDs COLDCH-11, COLDCH-12, F16, F12(curve), PWM-17, CYC-12, CYC-13, STARTUP-10, MODS-12, DHW-11, S13) and #69 (ADR-069/071 wrong values, ADR-195 boundary/CH-rule wording): the reference docs contradict the code and present questions he already answered (entries 1-35) as open, so he could be asked twice. Docs only; Accepted ADRs are immutable: record ADR errors as a note in sat-comparison.md / a follow-up task, not by editing them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every item listed under audit #36 is corrected in sat-comparison.md / firmware-sat-spec.md with a citation (code file:line, entry N), checked by a second reader agent that opens each citation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 workflow wf_9011968e-00d: fix agent made 15 changes (each with '(corrected 2026-10-24, TASK-1351)'); 15 second readers opened every citation: 10 changes refuted (stale/wrong line citations, leftover sentences, 1.5 coefficient left, imprecise 'PWM OFF phase' wording, ADR-195 caveat missing), 5 confirmed; repair agent fixed all 10 (67 exact-match edits), citations refreshed to HEAD 2d01d3297. Not second-read again after the repair. Left as is: TASK-1331 rows (not in the tree, noted), heat-pump PWM exclusion vs entry 21 marked open, summer-simmer PWM effect open. Found outside scope: stale code comments SATcontrol.ino:5206-5207 ('MM=100 restore', 'below'), :5185-5186 ('3.0C margin'), :130-131 ('below'); entries 12 and 24 'Open' lines now outdated (append-only log, not rewritten).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
sat-comparison.md and firmware-sat-spec.md now match the code and Sergeant D's entries for every item of audit #36 (cold cutoff/CH/MM, solar order and freeze, valves closed, heat-pump PWM, PM chain, adopt rule, classifier cold setpoint, DHW lines, plus the tasks shipped today), each change dated and cited. Evidence: second-reader pass over all 15 changes, 10 refuted and repaired. Not re-read after the repair; stale code comments listed in the notes for a follow-up.
<!-- SECTION:FINAL_SUMMARY:END -->
