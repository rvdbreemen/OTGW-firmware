---
id: TASK-794
title: 'Release skill: commit version-banner sweep, not just version.h'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 22:00'
updated_date: '2026-05-31 22:01'
labels:
  - release
  - tooling
  - docs
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Process fix found during the v1.6.1 release: build.py runs autoinc-semver with --update-all, which rewrites the 'Version :' banner comments across ~24 source and data files (FSexplorer.ino, MQTTstuff.ino, data/*.css/js, etc.) in addition to version.h and data/version.hash. The release skill (.claude/skills/release/SKILL.md) Phase 5 step 4 ('Commit the release build') and Phase 2 step 3 only mention version.h, so the banner-rewritten files were left uncommitted on main during v1.6.1; main HEAD now carries stale 'v1.6.1-beta' banners in source comments (binary version is correct via version.h). Update the skill so the build-output commit stages ALL files the build rewrote and verifies a clean tree before tagging. Also add a note to avoid running git mutations (checkout/stash/merge) in parallel tool calls.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Phase 5 build-output commit step explicitly stages every file the build rewrote (version.h, version.hash, and the autoinc-semver version-banner sweep across src/ and data/), e.g. via staging src/OTGW-firmware/ wholesale
- [x] #2 Phase 5 adds a verification that 'git status' shows a clean tree (no leftover banner/build changes) before creating the tag/release
- [x] #3 Phase 2 step 3 carries the same all-build-output staging guidance
- [x] #4 A note is added warning against running git checkout/stash/merge in parallel tool calls (serial only)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed two release-workflow process flaws found during v1.6.1 in .claude/skills/release/SKILL.md:

1. Banner sweep: Phase 2 step 3 and Phase 5 step 4 now state that build.py runs autoinc-semver --update-all, which rewrites version.h, data/version.hash AND the 'Version :' banner comments across ~24 source/data files. Both steps now stage the whole sweep via 'git add src/OTGW-firmware/' and verify 'git status' is clean before proceeding/tagging. Added a matching rule to Important rules.
2. Parallel git: added an Important rule that git mutations (checkout/stash/merge/commit) must run serially, never in parallel tool calls (they race on index/working-tree state); only read-only git queries may overlap.

Root cause of the original flaw: the skill said 'commit version.h' so only version.h/version.hash were staged on main during v1.6.1, leaving stale -beta banners in source comments on the published tag (binary version is correct via version.h). Docs/skill-only change.
<!-- SECTION:FINAL_SUMMARY:END -->
