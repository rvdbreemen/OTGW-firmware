---
id: TASK-1187
title: >-
  docs(adr): give the 'no String in hot paths' rule an Accepted ADR home
  (ADR-004 is superseded)
status: To Do
assignee: []
created_date: '2026-10-01 04:24'
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
- [ ] #1 An Accepted ADR states the 'no String class in hot paths' rule and its hot-path file list (a new ADR or an amendment to ADR-053, authored through adr-kit)
- [ ] #2 evaluate.py, CLAUDE.md and AGENTS.md cite that ADR instead of ADR-004
- [ ] #3 If the rule is meant to be binding, its frontmatter says binding: true with its gate, and the CLAUDE.md/AGENTS.md binding lists and tests/test_adr_governance.py agree
<!-- AC:END -->
