---
id: TASK-794
title: 'Release skill: commit version-banner sweep, not just version.h'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 22:00'
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
- [ ] #1 Phase 5 build-output commit step explicitly stages every file the build rewrote (version.h, version.hash, and the autoinc-semver version-banner sweep across src/ and data/), e.g. via staging src/OTGW-firmware/ wholesale
- [ ] #2 Phase 5 adds a verification that 'git status' shows a clean tree (no leftover banner/build changes) before creating the tag/release
- [ ] #3 Phase 2 step 3 carries the same all-build-output staging guidance
- [ ] #4 A note is added warning against running git checkout/stash/merge in parallel tool calls (serial only)
<!-- AC:END -->
