---
id: TASK-1634
title: Move SAT research scripts out of scripts/
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:44:00Z'
updated_date: '2026-11-05T12:26:00Z'
labels:
  - process
  - agentic
  - sat
dependencies: []
priority: low
ordinal: 761000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F7. 45 of 84 files in scripts/ are sat_* studies and simulations (study 2..25, hp studies), next to operational tooling (discord-post-build.sh, ota_flash.py). Many are referenced from docs/sat/ (sim-hp-lowload/round*.md, verification plan, crosswalk, resolution index). Moving them is only worth it when every reference moves along.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Research scripts live under tools/research/sat/ (or next to their docs/sat/sim-* folder); operational SAT tools (sat_boiler_emulator.py, sat_log_to_csv.py, verify_*) stay
- [x] #2 Every reference in docs/, tests/ and scripts/ is updated; grep for the old paths finds nothing
- [x] #3 python scripts/run_tests.py --no-host passes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05, Robert (AskUserQuestion): deferred until the SAT campaign is quiet (after TASK-1566 / TASK-1412). Reason: other sessions use these scripts right now (9 commits touched scripts/sat_* in the two days before), and moving them mid-study breaks their paths. Do not pick up before then.

2026-11-05 Robert (AskUserQuestion 'Beide uitvoeren'): do it now, despite the earlier deferral. Moved 42 research scripts with git mv to tools/research/sat/ (45 renames in git incl. siblings; operational tools stay in scripts/: sat_boiler_emulator, sat_log_to_csv, sat_corrections_index, sat_open_questions, sat_bench_scenarios_1412_20, sat_transport_delay_1412_5). Reference rewrite by script over docs/, tests/, scripts/, tools/, .claude/, .github/ and the agent files: 63 files. Path-depth fixes: sat_sim_plots ROOT, sat_stock_houses DATA, sys.path in three tests. Not rewritten on purpose (immutable records): docs/sat/sergeant-d-corrections.md and ADR-205 (Accepted); tools/research/sat/README.md says how to read the old paths there. Evidence: the six affected tests (closedloop_sim_1412_19, lowload_study_1412_16, sim_port_equivalence, bench_scenarios_1412_20, log_to_csv, corrections_index) exit 0 before and after the move; py_compile of all moved files OK; sat_stock_houses.DATA resolves to an existing file; grep for scripts/sat_<moved> outside backlog/.wolf finds only ADR-205 line 233.

Correction to the note above: after reverting the two immutable records, grep for scripts/sat_<moved> outside backlog/ and .wolf/ finds 7 hits, all in immutable records: docs/sat/sergeant-d-corrections.md (5) and ADR-205 (2). Full suite (python scripts/run_tests.py --no-host, worktree lane-1631-1634 after the move): 138 files, 133 passed, 4 failed, 1 skipped. Failures checked: test_sat_lowload_contract LL-023 fails identically on a clean origin/dev worktree (d08cd47a6, pre-existing); test_static_integrity and test_refresh_storm pass when run alone (load in the -j 4 run); test_capture_tool is flaky on both trees (lane: 1 pass and 1 fail in two solo runs; clean origin/dev: 1 pass and 1 fail with 2 failures). None touches the moved scripts.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Moved 42 SAT research scripts from scripts/ to tools/research/sat/ with git mv (history kept, 45 renames detected), plus tools/research/sat/README.md. The six operational SAT tools stay in scripts/. References rewritten in 63 files under docs/, tests/, scripts/, tools/ and .claude/; path-depth fixes in sat_sim_plots, sat_stock_houses and three tests. AC1: git status shows the renames; py_compile of all moved files OK. AC2: grep for the old paths finds only the immutable records (sergeant-d-corrections.md 5, ADR-205 2), left unchanged on purpose and explained in the README. AC3: the six affected tests pass before and after the move; full suite --no-host 133/138 passed, the 4 failures are pre-existing (LL-023, same on clean origin/dev) or load/flaky (static_integrity, refresh_storm pass solo; capture_tool flaky on clean origin/dev too). Deferral lifted by Robert on 2026-11-05 ('Beide uitvoeren'). Risk: sessions that run a study by its old scripts/ path need the new path.
<!-- SECTION:FINAL_SUMMARY:END -->
