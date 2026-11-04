---
id: TASK-1629
title: Correct the status column of docs/sat/open-questions.md and test it
status: To Do
assignee: []
created_date: '2026-11-04T22:41:00Z'
labels:
  - process
  - agentic
  - sat
dependencies: []
priority: medium
ordinal: 756000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F5 (part). Rows in the question table still say 'asked' or 'queued' while the Answered column already names an entry (examples: Q-1519b -> 163, Q-1519d -> 172, Q-1519e -> 173, Q-1531a -> 188, Q-1537a -> 233). The 'Update' paragraphs at the top have piled up as history.
Documentation hygiene only: no SAT behaviour, no question to Sergeant D. Do not rewrite entries in sergeant-d-corrections.md (they are immutable).
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every row whose Answered column has a checkmark and entry has status answered (or dropped with a reason)
- [ ] #2 The stacked 'Update' paragraphs move to a 'History' section at the bottom; the top states only the current live queue
- [ ] #3 A test in tests/ fails when a row has an answered entry but status asked/queued; red on the current file, green after the fix
<!-- AC:END -->
