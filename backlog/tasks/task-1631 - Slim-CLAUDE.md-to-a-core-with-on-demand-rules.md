---
id: TASK-1631
title: Slim CLAUDE.md to a core with on-demand rules
status: To Do
assignee: []
created_date: '2026-11-04T22:42:00Z'
updated_date: '2026-11-05T04:49:00Z'
labels:
  - process
  - agentic
dependencies:
  - TASK-1625
priority: medium
ordinal: 758000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F4. CLAUDE.md has 673 lines with 14 dated maintainer directives and is loaded in full in every session (with AGENTS.md about 120 KB). Some rules replace earlier ones but the old text stays (build cadence 2026-10-29 -> 10-30, SAT question rhythm 10-24 -> 10-25). This costs context on every turn and invites conflicting readings.
Plan checkpoint: the split is a maintainer choice. Present the proposed core and the moved sections to Robert with AskUserQuestion before moving anything.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert approved the split (core vs on-demand) in the session
- [ ] #2 CLAUDE.md core is at most about 200 lines: roles, hard prohibitions, gates, where to find the rest
- [ ] #3 Domain rules (SAT ground truth, build/bench cadence, release, Discord) live in .claude/rules/ or skills and are referenced from the core
- [ ] #4 Superseded directives are removed from the running text and listed in a dated directive changelog
- [ ] #5 The TASK-1625 drift test and tests/test_adr_governance.py still pass
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05, Robert (AskUserQuestion): deferred until a quiet moment after the beta phase. Options offered: a light clean-up pass (superseded directives to a changelog, ~690 to ~550 lines) or a full split into path-scoped .claude/rules (~200-250 line core). Facts at the time: CLAUDE.md 690 lines, 70 KB; largest blocks test automation 61, navigation 47, versioning 41, build 39, ADR guidelines 37; the ADR list and SAT ground truth blocks must stay (drift tests), and the AGENT-CORE test (TASK-1625) already guards the shared rules. Before a split, verify that this Claude Code version loads .claude/rules/*.md with a paths: frontmatter only for matching files.
<!-- SECTION:NOTES:END -->
