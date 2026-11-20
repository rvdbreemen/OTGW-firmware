---
id: TASK-2932
title: >-
  OLED device page shows 'FW 2.0.0-_VERSION_PRERELEASE' in a production build
  (unguarded stringify of the prerelease macro)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T20:25:00Z'
updated_date: '2026-11-19T21:55:00Z'
labels:
  - oled
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1065000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-2929 production dry run (otgw-dev, 2026-11-19): OLED.ino:358 formats the device page with PSTR("FW %d.%d.%d-" OLED_STR(_VERSION_PRERELEASE)) without a guard. With _VERSION_PRERELEASE undefined (every stable release) OLED_STR stringifies the bare token, so the device page reads 'FW 2.0.0-_VERSION_PRERELEASE'. Evidence: the dry-run image 2.0.0+d6544e1 .ino.bin holds the string '_VERSION_PRERELEASE' once (RvdB/OTGW-worktree-leftovers/prod-dryrun-d6544e1). Sweep of src/, data/ and the build scripts: this is the only unguarded use in firmware code; the scripts (autoinc-semver.py, bump-prerelease.sh, discord-post-build.sh, alpha_ledger.py) read version.h on purpose and handle or refuse a production version.h.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Production builds show 'FW <major>.<minor>.<patch>' and prerelease builds keep 'FW <x.y.z>-<prerelease>'
- [x] #2 A host test fails on any use of _VERSION_PRERELEASE in src/OTGW-firmware outside a positive #if defined(_VERSION_PRERELEASE) block, a preprocessor condition, a comment or version.h; red on today's OLED.ino:358, green on the fix
- [x] #3 The production dry run of the fixed build has '_VERSION_PRERELEASE' 0 times in its .ino.bin; bench (otgw-bench-tester): OLED device page checked if the unit has an OLED
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 otgw-dev: OLED.ino device page formats 'FW %d.%d.%d-<prerelease>' only under #if defined(_VERSION_PRERELEASE), else 'FW %d.%d.%d'. Sweep of src/, data/ and the build scripts: OLED.ino:358 was the only unguarded use in firmware code; the scripts read version.h on purpose. New tests/test_prerelease_macro_guarded.py scans every firmware source with a preprocessor block stack: red on the old OLED.ino (1 failing: OLED.ino:358), green 2/2 on the fix (incl. a self-test of the scanner on the stringify, #else and comment cases). AC3 (production dry run 0x '_VERSION_PRERELEASE' + OLED check) comes with the 401 dry run.

2026-11-19 production dry run 2.0.0+47d08b1: .ino.bin has '_VERSION_PRERELEASE' 0x and the format 'FW %d.%d.%d' 1x; the production image ran on .61 (09's 2931 bench). The OLED screen itself was not looked at (not part of 09's report).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OLED device page: 'FW %d.%d.%d-<prerelease>' only under #if defined(_VERSION_PRERELEASE), 'FW %d.%d.%d' otherwise; before, a stable build printed 'FW 2.0.0-_VERSION_PRERELEASE'. AC1/AC2: tests/test_prerelease_macro_guarded.py (preprocessor-stack scanner over all firmware sources): red on the old OLED.ino:358, green 2/2; it guards against any new unguarded use. AC3: production dry run 2.0.0+47d08b1 .ino.bin holds '_VERSION_PRERELEASE' 0x and 'FW %d.%d.%d' 1x; the image ran on .61. Not verified: the OLED screen itself.
<!-- SECTION:FINAL_SUMMARY:END -->
