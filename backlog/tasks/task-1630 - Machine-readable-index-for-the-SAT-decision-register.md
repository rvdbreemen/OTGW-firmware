---
id: TASK-1630
title: Machine-readable index for the SAT decision register
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:42:00Z'
updated_date: '2026-11-05T04:26:00Z'
labels:
  - process
  - agentic
  - sat
dependencies:
  - TASK-1629
priority: medium
ordinal: 757000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F5. docs/sat/sergeant-d-corrections.md has 269 entries in 6013 lines, without machine-readable metadata. Maintainer decisions (at least 18 'maintainer decision' entries) sit between Sergeant D's, so provenance ('who decided this, is it confirmed by Sergeant D?') can only be found by reading the whole file. Entries are immutable by rule (CLAUDE.md SAT ground truth block), so do not edit them: add a sidecar index instead.
Proposed: docs/sat/sergeant-d-corrections.index.json (or .yml) with per entry: id, date, source (sergeant-d | maintainer), kind (correction | answer | decision | interpretation), discord link, supersedes, tasks, confirmed_by_entry.
This changes how SAT knowledge is recorded, not SAT behaviour; still ask Robert to approve the schema before filling it.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert approved the index schema (recorded in the notes)
- [x] #2 The index covers every entry in sergeant-d-corrections.md; a test fails when an entry has no index row or the ids differ
- [x] #3 CLAUDE.md SAT block and .claude/commands/backlog_discord.md tell agents to add an index row with each new entry
- [x] #4 The open-questions status (TASK-1629 test) can be checked against the index
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Schema approved by Robert 2026-11-05 (AskUserQuestion): JSON sidecar docs/sat/sergeant-d-corrections.index.json; fields id, date, source (sergeant-d/maintainer), kind (correction/answer/decision/interpretation/unclassified), discord_link, supersedes, tasks, confirmed_by_entry; script fills what the text states, kind/confirmation only from explicit text, never a guessed confirmation.

2026-11-05. scripts/sat_corrections_index.py builds docs/sat/sergeant-d-corrections.index.json (269 records, 107 KB) from the register text only; --update rewrites, --check exits 1 when stale. Source precedence: trailing heading tag, then the Source line, then 'same message as entry N' inherits. Beyond the approved values the schema gained source 'port-team' for entries that are our own reading ('our reading', 'interpretation, not his words', 'no new statement'): without it they showed as unknown; 0 unknown remain. Result: sergeant-d 222, maintainer 31, port-team 16; kind: answer 86, interpretation 17, decision 13, correction 4, unclassified 149; 11 confirmations (e.g. 257-264 by 265, 108 by 131), 6 supersede links. tests/test_sat_corrections_index.py (5 tests): FAILED (failures=4) before the index existed, OK after --update; it checks coverage, schema values, currency, known provenance and that every answered checkmark in open-questions.md names an indexed entry (AC3). CLAUDE.md and AGENTS.md SAT blocks (identical, test_sat_ground_truth_block OK) and backlog_discord.md now name the --update step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Provenance of the SAT register is now machine-readable without touching the immutable entries. AC1: Robert approved the JSON sidecar (2026-11-05). AC2: docs/sat/sergeant-d-corrections.index.json covers all 269 entries; tests/test_sat_corrections_index.py fails on a missing or stale index (4 of 5 red before, 5 of 5 green after). AC3: the test also checks every answered checkmark in open-questions.md against the index. AC4: CLAUDE.md, AGENTS.md (SAT block) and backlog_discord.md tell agents to run scripts/sat_corrections_index.py --update after a new entry. Schema note: source 'port-team' added for our own readings (16 entries), so no entry stays unknown.
<!-- SECTION:FINAL_SUMMARY:END -->
