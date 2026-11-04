---
id: TASK-1631
title: Slim CLAUDE.md to a core with on-demand rules
status: To Do
assignee: []
created_date: '2026-11-04T22:42:00Z'
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
