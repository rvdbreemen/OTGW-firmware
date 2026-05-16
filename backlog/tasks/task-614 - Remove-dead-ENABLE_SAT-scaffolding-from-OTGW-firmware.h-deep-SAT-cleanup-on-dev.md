---
id: TASK-614
title: >-
  Remove dead ENABLE_SAT scaffolding from OTGW-firmware.h (deep SAT cleanup on
  dev)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 12:47'
updated_date: '2026-05-16 12:49'
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
- [x] #1 All 8 #if defined(ENABLE_SAT) blocks removed from OTGW-firmware.h (forward decls, addCommandToQueue alias, bSAT debug flag, SAT enums/structs, SATRuntimeSection+member, hasOTCommandInterface, SATSection+member)
- [x] #2 Zero ENABLE_SAT / SAT-subsystem references remain anywhere under src/OTGW-firmware/ (firmware + data)
- [ ] #3 python build.py --firmware exits 0 (firmware compiles clean after removal)
- [x] #4 python evaluate.py --quick shows no new failures vs baseline
- [x] #5 Prerelease version bumped per versioning policy (firmware source changed)
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Removed 8 dead #if defined(ENABLE_SAT) regions from OTGW-firmware.h via deterministic state-machine filter (348 lines; 898 -> 550). Verified: 0 ENABLE_SAT and 0 SAT-subsystem refs under src/OTGW-firmware/ (firmware+data); block boundaries join cleanly (addOTWGcmdtoqueue->sendLogToWebSocket, OTGWState/OTGWSettings structs intact, no dangling braces). Prerelease beta.7 -> beta.8 (autoinc-semver cascade, same file-set pattern as prior SAT-removal #586). evaluate.py --quick: identical to pre-change baseline (Failed:1 = 4 pre-existing unresolved ADR refs, unrelated to this change) -> no new failures. AC3 (python build.py --firmware) NOT self-verifiable here: remote env network policy blocks downloads.arduino.cc (HTTP 403), arduino-cli cannot install. Change is compile-inert by construction (deleted blocks were gated by ENABLE_SAT, never defined -> never in any TU). PR CI covers the real compile.
<!-- SECTION:NOTES:END -->
