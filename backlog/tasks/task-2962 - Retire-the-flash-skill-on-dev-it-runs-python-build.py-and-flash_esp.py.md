---
id: TASK-2962
title: 'Retire the /flash skill on dev: it runs python build.py and flash_esp.py'
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T14:44:00Z'
updated_date: '2026-11-20T14:52:00Z'
labels:
  - repo-hygiene
  - hard-rule
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1096000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F2 of the repo cleanup plan (docs/plans/2026-11-20-repo-cleanup-plan.md, TASK-2955). .claude/skills/flash/SKILL.md:17-20 runs 'python build.py' and 'python flash_esp.py --build --no-interactive'. On dev that conflicts with two CLAUDE.md hard rules: build only through build.bat / ./build.sh, and only the bench-tester (otgw-bench-tester) flashes a unit. The skill has disable-model-invocation: true, but a user typing /flash still runs it. The 1.x copy on otgw-1.x.x is not touched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The /flash skill on dev no longer runs a build or a flash: it is a stub that points to the bench-run request flow in docs/guides/agent-build-and-bench.md (or removed, with no reference left: git grep shows no /flash skill reference outside history)
- [ ] #2 python evaluate.py --quick shows no new FAIL; python scripts/run_tests.py green (tests that read .claude/skills stay green)
<!-- AC:END -->
