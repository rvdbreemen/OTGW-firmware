---
id: TASK-1074
title: release-assets.yml cannot attach assets to an immutable release after publish
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-10 20:47'
updated_date: '2026-08-10 21:13'
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
- [x] #1 The /release skill and docs/process/RELEASE_PROCESS.md generate SHA256SUMS, RELEASE_ASSETS.md, the capture scripts and the flash-bundle zip and attach them to the DRAFT before publishing
- [x] #2 release-assets.yml either runs pre-publish or is removed, so no release depends on a post-publish upload that cannot succeed
- [x] #3 A release dry-run confirms flash_otgw.sh auto-download verifies against SHA256SUMS from releases/latest/download
- [x] #4 docs/process/RELEASE_PROCESS.md documents that a published immutable release permanently reserves its tag name, so a deleted release cannot be republished under the same tag
- [x] #5 The false premise is corrected everywhere it is stated: the comment at .github/workflows/release-assets.yml:6 claiming 'Adding assets to an immutable release is permitted; only deleting is not', and the KennisBank note immutable-release-breekt-asset-upload
- [x] #6 No release step can report success while having attached nothing: either the workflow is gone, or it fails loudly when an expected asset is absent rather than skipping silently
- [x] #7 The workflow_dispatch backfill path is removed or documented as impossible, since assets cannot be added to any already-published release
<!-- AC:END -->



## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add scripts/make_release_assets.py: generates SHA256SUMS, RELEASE_ASSETS.md and the flash-bundle zip from build/ output (stdlib hashlib+zipfile, no zip binary needed), and prints the exact gh release create asset list.
2. Delete .github/workflows/release-assets.yml (cannot ever succeed post-publish; its workflow_dispatch backfill is impossible too).
3. Fix the stale cross-references in beta-prerelease.yml and .claude/skills/beta-prerelease/SKILL.md that point at the deleted workflow.
4. Rewrite RELEASE_PROCESS.md Phase 7 to generate assets and attach ALL of them to the DRAFT, with an explicit asset-count verification before publish.
5. Document the two immutable-release facts in RELEASE_PROCESS.md: no asset can be added after publish, and a published tag name is permanently reserved even after deleting the release.
6. Mirror the same steps in .claude/skills/release/SKILL.md Phase 5.
7. Correct the KennisBank note immutable-release-breekt-asset-upload.
8. Verify: run the generator against the current build/ output and confirm it reproduces the v1.7.4 assets byte-for-byte on SHA256SUMS content.
<!-- SECTION:PLAN:END -->
