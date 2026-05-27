---
id: TASK-614
title: >-
  Remove dead ENABLE_SAT scaffolding from OTGW-firmware.h (deep SAT cleanup on
  dev)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-16 12:47'
updated_date: '2026-05-16 12:56'
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
- [x] #3 python build.py --firmware exits 0 (firmware compiles clean after removal)
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

CI "Run evaluate.py --quick" failed on PR #589 — diagnosed as PRE-EXISTING on origin/dev, NOT a regression. Proof: clean origin/dev worktree runs evaluate.py --quick -> exit 1, Failed:1, "4 unresolved ADR reference(s) out of 1230"; my branch -> byte-identical (exit 1, same Failed:1, same 4 refs). git diff origin/dev HEAD -- evaluate.py docs/adr/ is empty (PR touches neither). evaluate.py exits 1 on any failure and the dev branch already carries this 1 failure (stale ADR cross-refs in docs, unrelated to SAT). Out of scope for a dead-code-removal PR; not fixing here. Recommend a separate backlog task to resolve the 4 dangling ADR references on dev.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed all dead SAT scaffolding from the dev branch. Prior PR #586 deleted the SAT *.ino implementation files but left orphaned type/struct/forward-declaration scaffolding in src/OTGW-firmware/OTGW-firmware.h behind #if defined(ENABLE_SAT); ENABLE_SAT is never defined anywhere on dev, so all 8 guarded regions were permanently-dead code with zero references in any .ino/.cpp/.js/.html.

What changed: deleted 8 #if defined(ENABLE_SAT) regions (348 lines; OTGW-firmware.h 898 -> 550): addCommandToQueue alias, SAT + weather forward declarations, bSAT debug flag, SAT runtime enums/structs, SATRuntimeSection (+ state.sat member), hasOTCommandInterface, SATSection (+ settings.sat member). Prerelease bumped beta.7 -> beta.8 (autoinc-semver cascade, same file-set pattern as #586).

Verification: 0 ENABLE_SAT and 0 SAT-subsystem refs remain under src/OTGW-firmware/ (firmware+data); struct boundaries join cleanly. evaluate.py --quick byte-identical to origin/dev baseline (1 pre-existing failure: 4 unrelated unresolved ADR refs — diagnosed, not introduced by this PR, out of scope). AC3 (python build.py --firmware) not locally self-verifiable (remote env network policy blocks downloads.arduino.cc -> arduino-cli cannot install); change is compile-inert by construction (deleted blocks were ENABLE_SAT-gated, never in any TU) and was accepted via PR #589 CI + maintainer merge into dev.

User impact: none (dead-code removal). Merged via PR #589.

Follow-up: pre-existing dev failure of 4 unresolved ADR references is unrelated to SAT and left for a separate task.
<!-- SECTION:FINAL_SUMMARY:END -->
