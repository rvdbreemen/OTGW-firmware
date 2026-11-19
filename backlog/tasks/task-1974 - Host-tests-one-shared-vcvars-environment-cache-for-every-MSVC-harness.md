---
id: TASK-1974
title: 'Host tests: one shared vcvars environment cache for every MSVC harness'
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-15T13:38:00Z'
updated_date: '2026-11-18T20:20:00Z'
labels:
  - tests
  - tooling
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1956 (coordinator, 2026-11-15). Calling vcvars costs about 8 s; the MSVC host harnesses call it on every compile (bat files built on test_ot_reserved_range.find_vcvars(), the _vcvars()/compile_cpp() copies in the SAT tests, the .ps1 harnesses). One shared helper captures the vcvars environment once per machine (cache file under the gitignored generated dir, written atomically, keyed on the vcvars path) and hands every harness a fast replacement, so a compile no longer pays for vcvars.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 One shared helper in the test tree; find_vcvars() and every _vcvars() copy use it; the .ps1 harnesses either use it or are listed with the reason they do not
- [ ] #2 Same verdict lines per harness before and after (no case or mutant result changes)
- [ ] #3 Total run_tests.py time before and after, measured the same way (chunks, same -j), recorded in the notes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 parked (coordinator, lane audit): WIP on lane/ba-vcenv 2e4e44165, timing pending. The task file existed only in that WIP commit; it is put on origin/dev here so the task stays visible. Milestone 2.1.0 (test tooling, not needed for the 2.0.0 stable). Keep lane/ba-vcenv until 1974 is picked up.
<!-- SECTION:NOTES:END -->
