---
id: TASK-629
title: Bump dev version to 1.6.0
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-19 16:49'
updated_date: '2026-05-19 16:51'
labels:
  - versioning
  - release
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Promote the accumulated 1.5.1-beta.* dev line to a 1.6.0 minor version. The beta line added a new user-visible HA capability (button + select PIC-control entities, pseudo-ID 251 via #596) on top of MQTT/HA correctness fixes (ADR-073/074/075) and SAT code-surface cleanup; per semver a new capability warrants a MINOR bump rather than folding into a 1.5.1 patch. Update version.h major/minor/patch and recompute all cascaded _SEMVER_*/_VERSION fields plus data/version.hash via scripts/autoinc-semver.py.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 version.h shows _VERSION_MAJOR 1 / _VERSION_MINOR 6 / _VERSION_PATCH 0 and all _SEMVER_*/_VERSION lines consistently reflect 1.6.0 with the chosen prerelease tag
- [ ] #2 data/version.hash and version.h _VERSION_GITHASH/_VERSION_DATE/_VERSION_TIME regenerated via scripts/autoinc-semver.py (not hand-edited)
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures vs dev baseline
- [ ] #5 Change committed to claude/bump-version-mUmdS and a draft PR opened against dev
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Hand-edit src/OTGW-firmware/version.h: _VERSION_MINOR 5->6, _VERSION_PATCH 1->0 (autoinc-semver.py reads these, does not bump them)
2. Run python3 scripts/autoinc-semver.py src/OTGW-firmware --prerelease beta.1 to recompute _SEMVER_*/_VERSION, build#, githash, date/time and data/version.hash
3. Verify version.h is internally consistent (1.6.0-beta.1 everywhere)
4. python build.py --firmware (exit 0)
5. python evaluate.py --quick (no new failures)
6. Commit to claude/bump-version-mUmdS, push -u origin, open draft PR against dev
<!-- SECTION:PLAN:END -->
