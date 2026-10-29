---
id: TASK-1456
title: >-
  build.py: never more than 4 compiler jobs, also not through OTGW_BUILD_JOBS
  (maintainer 2026-10-29)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T06:33:00Z'
updated_date: '2026-10-29T06:51:00Z'
labels:
  - build
  - tooling
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 616000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: 'Over alle agents nooit meer dan 1 build tegelijk runnen, en nooit meer dan 4 processoren commiten in het build script.' build.py pio_jobs() (TASK-1335) defaults to min(8, cpu_count) and lets OTGW_BUILD_JOBS raise it without limit. Background builds get killed on commit charge when several agent sessions run. Change: PIO_MAX_JOBS = 4 and clamp the override to it; record the one-build-at-a-time and combo-only rules in CLAUDE.md, AGENTS.md and the implement-next-task skill.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 pio_jobs() returns at most 4 for any cpu count and any OTGW_BUILD_JOBS value (tests/test_build_jobs.py red on HEAD, green on fix)
- [x] #2 CLAUDE.md, AGENTS.md and .claude/skills/implement-next-task/SKILL.md state: one build at a time across all agents/sessions, at most 4 jobs, esp32-combo only on dev
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: PIO_MAX_JOBS 8 -> 4 and OTGW_BUILD_JOBS clamped to it. tests/test_build_jobs.py on old build.py: FAILED (failures=3: default_capped_at_4, invalid_override_falls_back, override_never_above_4); on fix: Ran 7 tests OK. Rules added to CLAUDE.md (Build Commands + push policy), AGENTS.md (Build and Verification Commands) and the implement-next-task skill step 5.3; tests/test_adr_governance.py OK (binding block still in sync). Real build with the new cap still to run (next combo build).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py caps compiler jobs at 4 (PIO_MAX_JOBS 4; OTGW_BUILD_JOBS can only lower it). Evidence: tests/test_build_jobs.py 3 fail on the old build.py, 7 OK now; the alpha.527 build without OTGW_BUILD_JOBS set ran 'platformio run -e esp32-combo --jobs 4' and '-t buildfs --jobs 4' (SUCCESS 10:48 + 0:30). CLAUDE.md, AGENTS.md and the implement-next-task skill record the one-build-at-a-time rule across all sessions, the 4-job cap, combo-only on dev, and the publish-every-alpha-tag protocol with the coordinator ledger.
<!-- SECTION:FINAL_SUMMARY:END -->
