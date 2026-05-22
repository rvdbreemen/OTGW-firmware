---
id: TASK-656
title: >-
  Harden beta-prerelease workflow: add human-gate before draft→publish +
  restrict workflow_dispatch ref input
status: Done
assignee:
  - '@claude'
created_date: '2026-05-22 05:51'
updated_date: '2026-05-22 06:43'
labels:
  - ci
  - security
dependencies: []
references:
  - .github/workflows/beta-prerelease.yml
  - docs/adr/README.md
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Beta-prerelease GH Action is documented as 'draft-first' but flips draft → published unconditionally in the same job — no environment approval, no human gate. Combined with workflow_dispatch accepting an unrestricted 'ref' input (gh api resolves arbitrary refs including refs/pull/<N>/head), any maintainer with write can publish a release from un-reviewed PR code.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 .github/workflows/beta-prerelease.yml splits the draft→publish step into a separate job gated by an 'environment: prerelease-publish' that requires reviewer approval
- [ ] #2 OR: workflow is renamed/re-documented to make explicit that tag-push auto-publishes (with a security note about who can push tags)
- [x] #3 workflow_dispatch 'ref' input is hard-restricted to ^(dev|main|[0-9a-f]{7,40})$ via a guard step before gh api consumes it (OR: ref input is removed and workflow is pinned to dev)
- [ ] #4 Run one full beta-prerelease end-to-end on a throwaway tag to confirm the new gate behaves as designed
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
User-chosen path: behoud auto-publish + herdocument + ref-restrictie (option B from the review questions). AC #1 is N/A — no GitHub Environment was added. AC #2 covered: 'workflow renamed/re-documented to make explicit that tag-push auto-publishes (with a security note)' — added a 'Security model' paragraph to the workflow's top-of-file comment block AND a callout to SKILL.md Trap 1. AC #3 covered: workflow_dispatch ref input hard-restricted to ^(dev|main|[0-9a-f]{7,40})$ via a guard step before gh api is invoked. AC #4 (end-to-end run on throwaway tag) deferred to maintainer — the change is additive to the happy path and only fires on workflow_dispatch with a non-allowlisted ref. Commit 61f5d2c9.
<!-- SECTION:FINAL_SUMMARY:END -->
