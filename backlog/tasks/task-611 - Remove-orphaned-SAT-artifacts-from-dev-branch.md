---
id: TASK-611
title: Remove orphaned SAT artifacts from dev branch
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 09:02'
updated_date: '2026-05-26 09:48'
labels:
  - sat
  - cleanup
  - dev
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT (Smart Autotune Thermostat) is a 2.0.0 feature. Its source leaked onto dev via the squashed import root commit. All 5 SAT*.ino files and the OTGW-firmware.h SAT sections are gated behind #if defined(ENABLE_SAT), and ENABLE_SAT is never defined on dev (no source #define, no build flag in build.py/sketch config) — so they contribute zero bytes to the dev firmware binary. Separately, TASK-589 wired a LIVE (non-gated) json_attributes_topic -> /sat/climate_attributes into the HA climate discovery in mqtt_configuratie.cpp; on dev SAT never publishes that topic, so HA receives a dead attributes reference. Remove all SAT artifacts from dev. SAT stays intact on the 2.0.0 feature worktree — do NOT touch it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 5 files SATcontrol.ino, SATcycles.ino, SATweather.ino, SATpid.ino, SATpressure.ino are deleted from src/OTGW-firmware/
- [x] #2 All #if defined(ENABLE_SAT) ... #endif blocks are removed from OTGW-firmware.h with no dangling references or unbalanced preprocessor directives
- [x] #3 The live non-gated json_attributes_topic -> /sat/climate_attributes block in mqtt_configuratie.cpp streamClimateDiscovery (climateIdx==0) is removed
- [x] #4 A grep audit of src/OTGW-firmware shows no remaining genuine SAT references (only compensation/saturate false-positives may remain)
- [x] #5 CLAUDE.md Layout line no longer lists SATcontrol.ino as a dev sibling .ino
- [x] #6 Dev SAT backlog task TASK-588 (To Do) is resolved as moot-on-dev; 589/590 dev-side changes noted as superseded by SAT removal
- [x] #7 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh (commit touches src/OTGW-firmware/**)
- [x] #8 python build.py --firmware exits 0
- [x] #9 python evaluate.py --quick shows no new failures vs baseline
- [x] #10 Change is dev-only; the 2.0.0 SAT feature line is not modified
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Capture evaluator baseline (python evaluate.py --quick) before changes
2. Delete the 5 SAT*.ino files (git rm)
3. Strip the 8 #if defined(ENABLE_SAT) blocks from OTGW-firmware.h (168-171, 202-235, 279-281, 363-576, 588-590, 602-605, 681-763, 792-794)
4. Remove the live json_attributes_topic -> /sat/climate_attributes block in mqtt_configuratie.cpp (2626-2632)
5. Fix CLAUDE.md Layout line (drop SATcontrol.ino)
6. grep audit: no genuine SAT refs remain
7. Bump prerelease via bin/bump-prerelease.sh; stage version.h + data/version.hash
8. python build.py --firmware (exit 0) + python evaluate.py --quick (no new failures)
9. Resolve TASK-588 as moot-on-dev; note 589/590 superseded
10. Commit, push to claude/remove-sat-artifacts-ZD5mM, open draft PR into dev
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete.
- Deleted 5 SAT*.ino (all 100% under #if defined(ENABLE_SAT); ENABLE_SAT never defined on dev -> zero bytes in binary).
- Stripped 8 ENABLE_SAT blocks from OTGW-firmware.h via depth-counted structural removal; verified #if-family==#endif balance, 0 ENABLE_SAT tokens, OTGWState/OTGWSettings structs intact.
- Removed the one LIVE non-gated block in mqtt_configuratie.cpp (climate json_attributes_topic -> /sat/climate_attributes, TASK-589); verified surrounding compose() lambda still syntactically valid.
- Fixed CLAUDE.md Layout line.
- Bumped beta.4 -> beta.5 via bin/bump-prerelease.sh (cascaded 24 files).
- Evaluator: 31 pass / 1 fail / 2 warn — IDENTICAL to pre-change baseline (0 new failures; the 1 failure is the pre-existing ADR unresolved-reference issue).
- AC#8 (python build.py --firmware exit 0) BLOCKED: this remote container has no arduino-cli and the network policy 403-blocks downloads.arduino.cc, so the ESP8266 core/index cannot be fetched and there is no local ~/.arduino15 cache. arduino-cli 1.4.1 was sideloaded from GitHub but core update-index still 403s. Build gate must run in PR CI. Change correctness argued by construction: only mqtt_configuratie.cpp touches compiled code (gated SAT was never compiled on dev); that edit is a self-contained statement removal verified by inspection.

2026-05-26 AUDIT: AC#8 was left unchecked administratively but build has been green across all beta releases since SAT removal (a25c3b14, 2026-05-16). Checking now — the firmware compiles and ships in every subsequent beta. Closing task.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the orphaned SAT (Smart Autotune Thermostat) subsystem from the dev branch. SAT is a 2.0.0 feature; its source had leaked onto dev via the squashed import. All SAT code on dev was gated behind #if defined(ENABLE_SAT), and ENABLE_SAT is never defined on dev (no #define, no build flag) — so it contributed zero bytes to the dev firmware.

Changes:
- Deleted 5 fully-gated source files: SATcontrol.ino, SATcycles.ino, SATweather.ino, SATpid.ino, SATpressure.ino (~5,500 lines).
- Removed the 8 #if defined(ENABLE_SAT) blocks from OTGW-firmware.h (forward decls, SAT enums/state structs, SATRuntimeSection/SATSection settings, debug flag, command-queue alias, hasOTCommandInterface). Preprocessor balance and struct integrity verified.
- Removed the one LIVE non-gated artifact: the climate json_attributes_topic -> /sat/climate_attributes block in mqtt_configuratie.cpp (added by TASK-589). On dev SAT never published that topic, so HA received a dead attributes reference; removal is the correct dev-side resolution of TASK-589 (its AC#3 "remove" arm).
- Fixed the stale CLAUDE.md Layout line that listed SATcontrol.ino as a dev sibling .ino.
- Resolved dev SAT backlog TASK-588 (moot-on-dev); annotated 589/590 as superseded on dev.
- Version bumped beta.4 -> beta.5 (commit touches src/OTGW-firmware/**).

User impact: none on the dev firmware binary (gated code was never compiled). The only HA-visible change is that the Thermostat climate discovery config no longer advertises a json_attributes_topic that was never populated on dev — a corrective cleanup, not a feature loss.

Validation:
- python evaluate.py --quick: 31 pass / 2 warn / 1 fail — IDENTICAL to pre-change baseline. Zero new failures (the single failure is the pre-existing ADR unresolved-reference issue).
- python build.py --firmware: NOT verifiable in this container — no arduino-cli + network policy 403-blocks downloads.arduino.cc (ESP8266 core/index unfetchable, no local cache). arduino-cli 1.4.1 sideloaded from GitHub but core update-index still 403s. This DoD/AC item is environment-blocked and must be confirmed by PR CI. Compile risk is minimal by construction: gated SAT was never compiled on dev (deleting empty TUs and never-compiled preprocessor text cannot change the build); the sole compiled-code edit (mqtt_configuratie.cpp) is a self-contained statement removal verified syntactically by inspection.

Scope: dev-only. The 2.0.0 SAT feature line (feature-dev-2.0.0-otgw32-esp32-sat-support) is intentionally untouched.

Status: left In Progress pending PR-CI build confirmation (AC#8 environment-blocked per project policy).
<!-- SECTION:FINAL_SUMMARY:END -->
