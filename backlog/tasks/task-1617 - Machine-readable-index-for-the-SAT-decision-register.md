---
id: TASK-1617
title: Machine-readable index for the SAT decision register
status: To Do
assignee: []
created_date: '2026-11-04 21:50'
updated_date: '2026-11-04 21:51'
labels:
  - process
  - agentic
  - sat
dependencies:
  - TASK-1616
priority: medium
ordinal: 757000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F5. docs/sat/sergeant-d-corrections.md has 269 entries in 6013 lines, without machine-readable metadata. Maintainer decisions (at least 18 'maintainer decision' entries) sit between Sergeant D's, so provenance ('who decided this, is it confirmed by Sergeant D?') can only be found by reading the whole file. Entries are immutable by rule (CLAUDE.md SAT ground truth block), so do not edit them: add a sidecar index instead.
Proposed: docs/sat/sergeant-d-corrections.index.json (or .yml) with per entry: id, date, source (sergeant-d | maintainer), kind (correction | answer | decision | interpretation), discord link, supersedes, tasks, confirmed_by_entry.
This changes how SAT knowledge is recorded, not SAT behaviour; still ask Robert to approve the schema before filling it.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1611.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert approved the index schema (recorded in the notes)
- [ ] #2 The index covers every entry in sergeant-d-corrections.md; a test fails when an entry has no index row or the ids differ
- [ ] #3 CLAUDE.md SAT block and .claude/commands/backlog_discord.md tell agents to add an index row with each new entry
- [ ] #4 The open-questions status (TASK-1616 test) can be checked against the index
<!-- AC:END -->
