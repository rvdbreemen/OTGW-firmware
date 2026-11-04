---
id: TASK-1629
title: Correct the status column of docs/sat/open-questions.md and test it
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:41:00Z'
updated_date: '2026-11-04T23:16:00Z'
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
- [x] #1 Every row whose Answered column has a checkmark and entry has status answered (or dropped with a reason)
- [x] #2 The stacked 'Update' paragraphs move to a 'History' section at the bottom; the top states only the current live queue
- [x] #3 A test in tests/ fails when a row has an answered entry but status asked/queued; red on the current file, green after the fix
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. Red on the old file: tests/test_open_questions_status.py FAILED (failures=2): 18 rows with an answering entry (checkmark) still asked/queued/confirmed/withdrawn/label kept, and no Live queue/History sections. Fix in two script passes (oq_fix.py, oq_fix2.py in the session scratchpad): every Status cell now starts with queued/asked/answered/dropped; the original text is kept after the new prefix (e.g. 'answered (entry 163): asked 2026-10-31 ...', 'dropped: fallen away: ...', 'answered (entry 256, decided by Robert): decided ...'). 36 rows changed in total (18 checkmark rows plus 18 off-vocabulary rows such as withdrawn/fallen away/informational/posted/decided). Q-1458a carried 'withdrawn' in its id cell; moved into Status as dropped. Asking order plus the five dated Update paragraphs moved under a new '## History' at the end; a '## Live queue' at the top lists the 4 open rows (QD-8, Conf-1371, Q-1436g queued; Q-1537b asked with partial answer ? 236). sergeant-d-corrections.md untouched. Green after the fix: 5 tests OK.

Regression check: only tests/test_sat_lowload_contract.py (test_ll018b, the Conf-1371 open-decision gate) reads open-questions.md (git grep over tests/ and scripts/). Run on the old file (origin/dev) and on the new file: identical, 'OK (expected failures=1)' both times; Conf-1371 stays queued. Full run scripts/run_tests.py --no-host: 120 passed, 4 failed, 1 timeout, 1 skipped; this docs-only diff cannot change any test except test_ll018b, shown identical above.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
open-questions.md status column made consistent and guarded by a test. AC1: every row with an answering entry now starts with answered or dropped (18 rows), plus 18 rows with off-vocabulary statuses (withdrawn, fallen away, informational, posted, decided) normalised; the original text stays after the new prefix. AC2: the asking order and the five dated Update paragraphs moved to a History section at the end; a Live queue at the top lists the 4 open rows (QD-8, Conf-1371, Q-1436g, Q-1537b). AC3: tests/test_open_questions_status.py (5 tests) FAILED on the old file (failures=2) and passes on the fix; it also checks the status vocabulary and that the status names the answering entry. sergeant-d-corrections.md untouched. Regression: test_ll018b identical old vs new (expected failure, Conf-1371 still open).
<!-- SECTION:FINAL_SUMMARY:END -->
