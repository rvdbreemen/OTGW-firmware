---
id: TASK-3042
title: >-
  Backlog hygiene: 29 duplicate task IDs block the CLI, renumber the colliding
  side (proposal, waits for Robert)
status: To Do
assignee: []
created_date: '2026-11-25T05:13:00Z'
updated_date: '2026-11-25T05:17:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1148000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
29 task IDs exist twice (59 files). The backlog CLI refuses them as ambiguous, so these tasks cannot be viewed, edited or closed. This task holds the proposal (which ID stays, what is renamed, how references are protected). Nothing is renumbered until Robert decides: the fix edits task files outside the CLI.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides option A, B or C (see notes); the decision is recorded here
- [ ] #2 If A: dry run reviewed, one commit, CLI views every kept and new ID, tests/test_backlog_unique_ids.py green and red on a seeded duplicate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
PROPOSAL 2026-11-25 (otgw-docs 95), nothing renumbered. Waiting for Robert: the fix edits archived and one active task file outside the backlog CLI (exception to the CLI-only rule).

PROBLEM. 29 task IDs exist twice (59 files: 29 in backlog/tasks, 30 in backlog/archive/tasks). The backlog CLI refuses them ("Task ID TASK-21 is ambiguous; 2 files match", checked for 21, 242 and 592), so these tasks cannot be viewed, edited or closed with the only tool agents may use. Open ones among the archived duplicates: TASK-21 (In Progress), TASK-26, 242, 243, 592 (To Do), TASK-19 test-task (To Do). Cause: two numbering series. Series A: the March 2026 refactor/MQTT tasks (archived, commit 23b07111d, 2026-03-19). Series B: the SAT import of 2026-04-05/06 that started again at TASK-1 (commits 4d482ab53, 55e8acf78). Three same-day collisions later: TASK-242, 243 (2026-04-09/10) and 592 (2026-05-08). TASK-19 also has a third file, task-19 - test-task.md, a CLI test leftover.

WHICH ID STAYS. Rule: the ID stays with the file that is referenced from outside the backlog; if neither is, with the file in backlog/tasks (the one the board shows). Measured on origin/dev 68973c00d: references outside backlog/ are ADR-197 (TASK-8, the PID integral, active series), ADR-092 (TASK-20, BLE scanner, active), ADR-101 line 128 (TASK-592 "archived": the OT-Thing interop, archived file), docs/sat/sat-port-audit-2026-10-24.md (TASK-3, active), docs/sat/sat-low-load-resolution-index-2026-10-29.md (TASK-14 and TASK-16 with their file paths, Robert's decision of that day, active), tests/test_sat_thermal_learning_gate.py (TASK-21, active) and 5 commits that name TASK-242 (all the active one). Other hits are synthetic test data (test_evaluate.py, test_agentic_throughput_waits.py, test_discord_post_build.py) and the placeholder TASK-9xx. ADRs are immutable, so their targets must keep their IDs. Result: 26 IDs of series B stay, TASK-242 and TASK-243 (active) stay, TASK-592 stays with the ARCHIVED file (ADR-101); the colliding side is renamed.

WHAT IS RENAMED. 29 files get a new ID: the 26 archived series-A tasks (TASK-1 to 26), the archived TASK-242 and TASK-243, and the ACTIVE TASK-592 (zone PID clamp fix, Done, no references). Their subtasks follow their parent: TASK-15.1 to 15.3 and TASK-22.1 to 22.3 (6 more files, parent_task_id changes). The test-task leftover (TASK-19, To Do) is deleted. New IDs are max+1 at execution time (3040 and up at the moment), assigned in old-ID order; the order is written into the mapping table. A reserved high block is no option: the CLI takes max+1, so one would push every new task to the block.

HOW REFERENCES ARE PROTECTED. (1) ADR, docs, tests and commit references need no change: they point at IDs that stay (list above). (2) Inside the backlog: 15 frontmatter references (dependencies, parent_task_id) and 38 notes references to the 29 IDs. The frontmatter ones inside a renamed series are rewritten by the script (task-4 depends on TASK-1, task-5 on TASK-4, task-9 and task-18 on TASK-2, task-14 on TASK-3, task-16 on TASK-4, task-21 on TASK-19 stay with series B and need no change; the 15.x and 22.x children and the archived dependencies are rewritten). Notes references are history and are not rewritten; most (TASK-14: 11, TASK-16: 6, TASK-21: 5, TASK-20: 3, TASK-3: 3) mean series B, which keeps its IDs. TASK-17, 18, 19, 22 in the notes of TASK-1005/1006/1007/1011 mean tasks of the separate adr-kit repo board, not ours. (3) Every renamed file gets one note line "renumbered from TASK-N (duplicate ID) on <date>" and the mapping table is committed as docs/process/task-id-renumbering-2026-11.md, so any old reference can be resolved. (4) The 1.x line and main carry the same archived files (11 of them there); they keep their old numbers. I did not verify that no flow merges backlog/ between the lines; the release flow merges otgw-1.x.x into main only.

HOW IT WOULD RUN. One script, dry run by default, that prints old to new and touches only: the id line, the file name (git mv), dependencies/parent_task_id of the renamed series, and one note line. One reviewed docs-only commit, run in a window where nobody runs "backlog task create" (max+1 race), pushed through a coordinator slot. Rollback: git revert of that commit. Afterwards the CLI must view every ID once (check: loop over all 29 kept and 29 new IDs) and a new test tests/test_backlog_unique_ids.py fails on any duplicate ID across backlog/tasks, archive/tasks, completed and drafts (no allow-list after the run). Tests run through scripts/run_tests.py; they are cheap and need no unit.

OPTIONS. A (recommended): renumber as above + guard test. Cost: one-off direct edit of 35 task files, an exception to the CLI-only rule. B: leave the files, commit the mapping and a guard test with an allow-list of the 29 IDs; cost: 29 IDs stay unusable through the CLI (the archived open ones cannot be closed), but no file edit. C: only delete the test-task and add the guard. Main risk of A: a mistake in the id/dependency rewrite; mitigated by the dry run, the single revert-able commit and the CLI check at the end. Not verified: a real run (the script does not exist yet; this task is the proposal).

MAPPING (old id, folder, status, title, proposal): see docs/process/task-id-renumbering-2026-11.md after the run; the draft table is in C:\Users\rvdbr\Claude\otgw-docs-prep\dupids-mapping-table.md (59 rows: 29 renumber, 29 keep, 1 delete). Scripts: dupids_scan.py, dupids_refs.py, build_proposal.py in the same folder.
<!-- SECTION:NOTES:END -->
