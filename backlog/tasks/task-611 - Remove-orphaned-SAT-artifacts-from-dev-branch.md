---
id: TASK-611
title: Remove orphaned SAT artifacts from dev branch
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 09:02'
updated_date: '2026-05-16 09:02'
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
- [ ] #1 The 5 files SATcontrol.ino, SATcycles.ino, SATweather.ino, SATpid.ino, SATpressure.ino are deleted from src/OTGW-firmware/
- [ ] #2 All #if defined(ENABLE_SAT) ... #endif blocks are removed from OTGW-firmware.h with no dangling references or unbalanced preprocessor directives
- [ ] #3 The live non-gated json_attributes_topic -> /sat/climate_attributes block in mqtt_configuratie.cpp streamClimateDiscovery (climateIdx==0) is removed
- [ ] #4 A grep audit of src/OTGW-firmware shows no remaining genuine SAT references (only compensation/saturate false-positives may remain)
- [ ] #5 CLAUDE.md Layout line no longer lists SATcontrol.ino as a dev sibling .ino
- [ ] #6 Dev SAT backlog task TASK-588 (To Do) is resolved as moot-on-dev; 589/590 dev-side changes noted as superseded by SAT removal
- [ ] #7 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh (commit touches src/OTGW-firmware/**)
- [ ] #8 python build.py --firmware exits 0
- [ ] #9 python evaluate.py --quick shows no new failures vs baseline
- [ ] #10 Change is dev-only; the 2.0.0 SAT feature line is not modified
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
