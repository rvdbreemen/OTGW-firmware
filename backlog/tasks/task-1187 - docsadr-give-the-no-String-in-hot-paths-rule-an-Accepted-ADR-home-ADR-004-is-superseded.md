---
id: TASK-1187
title: >-
  docs(adr): give the 'no String in hot paths' rule an Accepted ADR home
  (ADR-004 is superseded)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-01 04:24'
updated_date: '2026-10-01 16:44'
labels:
  - 2.0.0
  - adr
dependencies: []
priority: low
ordinal: 310000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1183. CLAUDE.md, AGENTS.md and evaluate.py cite ADR-004 for the rule 'no String class in hot paths' (SAT*, MQTTstuff, restAPI, OTGW-Core, OTDirect). ADR-004 (Static Buffer Allocation Strategy) is Superseded by ADR-053 (Large Feature Buffer Static Allocation), and ADR-053 does not restate the String rule. evaluate.py reports violations as WARN under the label 'String Class in Hot Path (ADR-004)'. Maintainer decision (2026-09-30): ADR-004 leaves the binding list and keeps binding: false; this task gives the rule an Accepted home.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 An Accepted ADR states the 'no String class in hot paths' rule and its hot-path file list (a new ADR or an amendment to ADR-053, authored through adr-kit)
- [x] #2 evaluate.py, CLAUDE.md and AGENTS.md cite that ADR instead of ADR-004
- [x] #3 If the rule is meant to be binding, its frontmatter says binding: true with its gate, and the CLAUDE.md/AGENTS.md binding lists and tests/test_adr_governance.py agree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-01:

ADR-180 'Keep the Arduino String class out of hot-path source files', Accepted 2026-10-01. It states:
- the rule;
- the hot-path file prefixes (SAT, MQTTstuff, restAPI, OTGW-Core, OTDirect);
- the reason (ADR-004 lines 30/41);
- the 10 current usages, all on on-demand paths: 7 in OTGW-Core PIC update, 2 REST argCompat(), 1 URI escape in an error path.

Guideline-level per ADR-080. Enforcement: require_pattern HOT_PATH_PREFIXES in evaluate.py. verified_in: evaluate.py:HOT_PATH_PREFIXES (adr document).

The maintainer chose guideline-level over binding-with-burn-down. The open question was answered with adr answer; acceptance via adr accept --confirm.

Citations moved to ADR-180:
- evaluate.py: comments, plus the result label 'String Class in Hot Path (ADR-180)'.
- CLAUDE.md: the Critical Coding Rules heading and the structural ADR list.
- AGENTS.md: String usage and the structural ADR list. The shared ADR block stays identical.
- evaluate.py:3584 keeps ADR-004 as the historical example in the ADR-080 gate docstring.

docs/adr/README.md:
- curated ADR-180 entry under Memory Management;
- ADR-004 annotation corrected from 'Extended by ADR-053' to 'Superseded by ADR-053; its String rule now lives in ADR-180'.

adr relate to ADR-004/053 was refused: neither has a Status History, and the tool will not invent one. ADR-180 names both under Related Decisions instead.

Two adr-kit findings, fixed before commit:
- accept rewrote the whole first Status line; the guideline-level label was restored as its own paragraph.
- 'supersedes ADR-004' in Related Decisions counted as ADR-180's own supersession claim; rephrased.

Evidence:
- adr-lint, all nine gates: PASS for ADR-180.
- tests/test_adr_governance.py: 15 OK.
- scripts/adr_governance.py lint: 0 fail (1 pre-existing warn, ADR-62); index-check: 0 fail.
- tests/test_evaluate.py: 71 OK.
- python evaluate.py: exit 0, 81 passed / 4 warnings / 0 failed (unchanged counts); the hot-path result is now labelled ADR-180.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gives the 'no String in hot paths' rule an Accepted home.

ADR-180 (guideline-level per ADR-080, Accepted 2026-10-01 by the maintainer) states the rule, the hot-path file prefixes, its reason and the 10 known usages, all on on-demand paths. evaluate.py, CLAUDE.md and AGENTS.md cite ADR-180 instead of the superseded ADR-004. ADR-180 joins the structural list in the shared CLAUDE/AGENTS ADR block, and the curated ADR index lists it.

Evidence:
- adr-lint: all gates PASS.
- test_adr_governance: 15 OK.
- test_evaluate: 71 OK.
- evaluate.py: exit 0, 81/4/0, unchanged.

AC#3: the maintainer chose guideline-level, so there is no binding entry, and the binding list (088/091/097/101/146/177) is unchanged.
<!-- SECTION:FINAL_SUMMARY:END -->
