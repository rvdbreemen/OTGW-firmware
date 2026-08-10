---
id: TASK-1074
title: release-assets.yml cannot attach assets to an immutable release after publish
status: To Do
assignee: []
created_date: '2026-08-10 20:47'
labels: []
dependencies: []
ordinal: 178000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
release-assets.yml triggers on release:published, but the repo enforces immutable releases, so every upload attempt fails with 'Cannot upload asset SHA256SUMS to an immutable release'. TASK-936 fixed the delete-before-upload problem via overwrite_files:false, but the remaining failure is ordering: after publish, no asset can be added at all. Stable releases v1.5.0 through v1.7.3 therefore shipped without SHA256SUMS, RELEASE_ASSETS.md, the capture scripts and the flash bundle, which makes flash_otgw.sh and flash_otgw.bat auto-download exit with EXIT_SHA_MISMATCH. v1.7.4 was fixed by generating the assets locally and attaching them to the draft before publishing, the same shape beta-prerelease.yml already uses. The release process and/or the workflow should make that the permanent path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The /release skill and docs/process/RELEASE_PROCESS.md generate SHA256SUMS, RELEASE_ASSETS.md, the capture scripts and the flash-bundle zip and attach them to the DRAFT before publishing
- [ ] #2 release-assets.yml either runs pre-publish or is removed, so no release depends on a post-publish upload that cannot succeed
- [ ] #3 A release dry-run confirms flash_otgw.sh auto-download verifies against SHA256SUMS from releases/latest/download
- [ ] #4 docs/process/RELEASE_PROCESS.md documents that a published immutable release permanently reserves its tag name, so a deleted release cannot be republished under the same tag
<!-- AC:END -->
