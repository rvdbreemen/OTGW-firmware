---
id: TASK-2955
title: 'Repo hygiene: clean up the OTGW-firmware repository without losing anything'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:05:00Z'
updated_date: '2026-11-21T00:23:00Z'
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
- [x] #1 Plan file docs/plans/2026-11-20-repo-cleanup-plan.md lists per item: current path/branch, target, action (keep/move/archive/delete), evidence, risk, owner
- [ ] #2 Branches: every deleted branch was landed in dev (or otgw-1.x.x) and in a published tag, or has an archive/<name> tag at its tip; git ls-remote shows the tag before the branch delete
- [ ] #3 Layout and 1.x moves: every moved path has its references updated (grep shows no stale path), published release-note links still resolve (kept or stub), python scripts/run_tests.py and python evaluate.py --quick show no new FAIL
- [ ] #4 .gitignore covers every artifact class found; git ls-files shows none of them tracked
- [ ] #5 No force-push, no history rewrite; HARD-keep branches (main, dev, otgw-1.x.x, release/2.x, research/cloud-recording, wt-09-bench, 5e telemetry lanes, and every lane the coordinator holds) untouched
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 1 done 2026-11-20 (otgw-architect): plan docs/plans/2026-11-20-repo-cleanup-plan.md merges three read-only inventories (branches/worktrees/tags; layout outside docs/ + 1.x + .gitignore; docs/** + root *.md by otgw-docs, 1685 files). Phase 2 = commits C1-C8 in order, after the 20:00 beta; decisions D1-D7 go to Robert through the coordinator. AC1 evidence: the plan lists per item the path/branch, target, action, evidence, risk and owner.

Robert, 2026-11-20, DM 1573349163083505787 (reply to the coordinator's D1-D7 question): "A" = all recommendations as listed. D1 per-branch archive tags only to the NAS backup remote; D2 delete GitHub branch task1757-staged-connect after an archive tag; D3 SAT sim output stays for 2.0.0; D4 remove the 17 generic Copilot skills, keep .github/skills/adr and fix docs/adr/README.md:1071; D5 untrack .claude/*_last_checked.txt; D6 leave docs/opentherm specification; D7 per-file 1.x docs check after the 2.0.0 stable. F2 split off as TASK-2962 (owner otgw-dev). Phase 2 starts after the 20:00 beta: coordinator tags archive/pre-cleanup-2026-11-20 on the dev tip, then C1 (coordinator), docs (otgw-docs), C3/C5 (otgw-dev, build slot).

Plan corrections from otgw-docs' dry run (2026-11-20): C6.7 keeps a stub (ADR-097 and two src comments cite the audit); ADR-020 note under C6.2; the F4 PINOUT link is not dangling; F6 goes to otgw-bench-tester through the coordinator.

2026-11-21 phase 2 C4 (otgw-docs): remove the duplicate .full-review-archive-20260421-085044 (3 files blob-identical to .full-review/archive/, state.json moved there)

2026-11-21 phase 2 C6.1 (otgw-docs): one plans folder: docs/plan/*, plan/ and the ps1 translator plan move to docs/plans/
<!-- SECTION:NOTES:END -->
