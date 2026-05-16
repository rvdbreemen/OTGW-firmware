---
id: TASK-614
title: >-
  Remove dead ENABLE_SAT scaffolding from OTGW-firmware.h (deep SAT cleanup on
  dev)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 12:47'
updated_date: '2026-05-16 12:47'
labels:
  - cleanup
  - tech-debt
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The prior SAT removal (#586 / its task) deleted the SAT*.ino implementation files but left orphaned type/struct/forward-declaration scaffolding in src/OTGW-firmware/OTGW-firmware.h, gated behind #if defined(ENABLE_SAT). ENABLE_SAT is never defined anywhere on the dev branch (no source #define, no build flag in build.py/platformio.ini), so all 8 guarded regions are permanently-dead code. No .ino/.cpp/.js/.html references any SAT identifier. Finish the cleanup by deleting these 8 dead blocks entirely so the dev branch carries no SAT residue.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All 8 #if defined(ENABLE_SAT) blocks removed from OTGW-firmware.h (forward decls, addCommandToQueue alias, bSAT debug flag, SAT enums/structs, SATRuntimeSection+member, hasOTCommandInterface, SATSection+member)
- [ ] #2 Zero ENABLE_SAT / SAT-subsystem references remain anywhere under src/OTGW-firmware/ (firmware + data)
- [ ] #3 python build.py --firmware exits 0 (firmware compiles clean after removal)
- [ ] #4 python evaluate.py --quick shows no new failures vs baseline
- [ ] #5 Prerelease version bumped per versioning policy (firmware source changed)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verified: ENABLE_SAT never defined; 8 self-contained #if defined(ENABLE_SAT) regions in OTGW-firmware.h; no nested preproc; zero external refs.
2. Strip all 8 inclusive #if defined(ENABLE_SAT)..#endif regions from src/OTGW-firmware/OTGW-firmware.h via deterministic state-machine filter.
3. Confirm 0 ENABLE_SAT / SAT-subsystem refs remain under src/OTGW-firmware/.
4. Bump prerelease (firmware source changed).
5. python build.py --firmware == 0; python evaluate.py --quick no new failures.
6. Commit to claude/cleanup-sat-code-l6rNl, push, open draft PR.
<!-- SECTION:PLAN:END -->
