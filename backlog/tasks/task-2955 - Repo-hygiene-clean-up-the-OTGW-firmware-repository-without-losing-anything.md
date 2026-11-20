---
id: TASK-2955
title: 'Repo hygiene: clean up the OTGW-firmware repository without losing anything'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:05:00Z'
updated_date: '2026-11-20T14:35:00Z'
labels:
  - repo-hygiene
milestone: 2.0.0
dependencies: []
ordinal: 1089000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Epic. Robert, 2026-11-20 (CLI, verbatim): 'Kan je het repo van OTGW-firmware kritisch laten doornemen, het kan zijn dat over de tijd nogal rommelig is geworden. Schoon het op, zonder iets te verliezen. Ruim oude branches die stale zijn op als de netjes in dev zitten. Zet documentatie en src en andere zaken netjes op de plek die daarvoor is bedacht. Archiveer oude informatie van de 1.x branch, oude teksten of logfiles die er nog zijn. Oftewel schoon de repository netjes op.' Phase 1 (otgw-architect, with otgw-docs for docs/**): read-only audit + plan docs/plans/2026-11-20-repo-cleanup-plan.md. Phase 2: execution after the 20:00 beta (freeze 18:45-20:00), one small commit per class, by the owner named per item (otgw-docs: docs moves; otgw-dev: src/scripts/build/tests; coordinator: branches). Nothing lost: every delete is preceded by an archive/<name> tag or a move into an archive folder; no history rewrite, no force-push. Lanes the coordinator holds are listed only, never touched.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Plan file docs/plans/2026-11-20-repo-cleanup-plan.md lists per item: current path/branch, target, action (keep/move/archive/delete), evidence, risk, owner
- [ ] #2 Branches: every deleted branch was landed in dev (or otgw-1.x.x) and in a published tag, or has an archive/<name> tag at its tip; git ls-remote shows the tag before the branch delete
- [ ] #3 Layout and 1.x moves: every moved path has its references updated (grep shows no stale path), published release-note links still resolve (kept or stub), python scripts/run_tests.py and python evaluate.py --quick show no new FAIL
- [ ] #4 .gitignore covers every artifact class found; git ls-files shows none of them tracked
- [ ] #5 No force-push, no history rewrite; HARD-keep branches (main, dev, otgw-1.x.x, release/2.x, research/cloud-recording, wt-09-bench, 5e telemetry lanes, and every lane the coordinator holds) untouched
<!-- AC:END -->
