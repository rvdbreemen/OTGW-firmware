---
id: TASK-936
title: >-
  Make flash scripts + SHA256SUMS reliably attach to every release (pre-publish
  gate)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-25 19:55'
updated_date: '2026-06-25 19:58'
labels:
  - release
  - tooling
  - docs
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Releases since v1.0.0 are GitHub immutable. The post-publish workflow .github/workflows/release-assets.yml runs on release:published, so its asset uploads are rejected (HTTP 422) and have failed on all 6 releases. v1.7.0 shipped with only the two .bin files: no flash_otgw.bat/.sh, no SHA256SUMS. The /release skill step 6 uploads the scripts to the draft but omits SHA256SUMS, and RELEASE_PROCESS.md omits both scripts and SHA256SUMS. Because the documented upload step demonstrably gets skipped, the fix is a hard pre-publish gate that blocks --draft=false unless the full asset set is present, plus folding SHA256SUMS generation into the manual draft-upload step, plus removing the dead post-publish workflow. ESP8266-only flash scripts, so 1.x line only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 C:/Program Files/Git/release SKILL.md Phase 5 generates SHA256SUMS from build bins and uploads it together with both .bin and both flash scripts to the DRAFT release before publish
- [ ] #2 RELEASE_PROCESS.md draft-upload step updated to match (bins + flash_otgw.sh + flash_otgw.bat + SHA256SUMS)
- [ ] #3 A hard pre-publish gate is added before 'gh release edit --draft=false' that aborts if any required asset (both bins, both scripts, SHA256SUMS) is missing from the draft
- [ ] #4 Dead workflow .github/workflows/release-assets.yml is removed (incompatible with immutable releases; its SHA256SUMS logic now lives in the manual process)
- [ ] #5 flash-bundle zip is intentionally dropped; confirmed no docs reference it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. /release SKILL.md Phase 5: split step 6 into (a) generate SHA256SUMS from build/*.bin, (b) upload bins+scripts+SHA256SUMS to draft, (c) hard pre-publish gate asserting all 5 assets present before --draft=false.\n2. RELEASE_PROCESS.md: same upload+gate update at step 7-9.\n3. Delete .github/workflows/release-assets.yml (dead, immutable-incompatible).\n4. Verify no doc references flash-bundle zip (done).\n5. Docs-only + CI removal: commit, push otgw-1.x.x.
<!-- SECTION:PLAN:END -->
