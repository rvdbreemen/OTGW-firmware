---
id: TASK-415
title: 'adr-kit v0.4.0: must-have polish (CHANGELOG, tag convention, .gitignore)'
status: Done
assignee: []
created_date: '2026-04-25 19:02'
updated_date: '2026-04-25 19:05'
labels:
  - adr-kit
  - external-repo
  - polish
  - must-have
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bring adr-kit to a "production-grade signal" baseline by adding the three must-have items from the best-practices review.

Scope:
1. CHANGELOG.md in Keep a Changelog format (https://keepachangelog.com/) with entries for v0.1.0, v0.2.0, v0.3.0, and the upcoming v0.4.0.
2. Migrate to the `<plugin-name>--vX.Y.Z` tag convention that `claude plugin tag` expects. Backward-compatible: also keep legacy `vX.Y.Z` tags on old releases so existing pinned installs do not break. New convention applies from v0.4.0 forward.
3. .gitignore with sensible defaults for Claude Code plugins (cache dirs, OS files, editor metadata).

Out of scope (handled in next tasks):
- CI workflow, CONTRIBUTING.md, manifest enrichment.
- Issue/PR templates, security policy, sample ADRs.

Both repos updated: rvdbreemen/adr-kit (canonical) and OTGW-firmware/tools/adr-kit (mirror).

Deliverables:
- adr-kit/CHANGELOG.md
- adr-kit/.gitignore
- Tags adr-kit--v0.1.0, adr-kit--v0.2.0, adr-kit--v0.3.0 (additional, on existing commits)
- Tag adr-kit--v0.4.0 (new release commit)
- GitHub Release v0.4.0 with notes mentioning the tag-convention migration and the new CHANGELOG.
- Synced mirror in OTGW-firmware/tools/adr-kit/.</description>
<parameter name="acceptanceCriteria">["adr-kit/CHANGELOG.md exists in Keep a Changelog format with at least four version sections (0.1.0, 0.2.0, 0.3.0, 0.4.0)", "adr-kit/.gitignore exists with at least: macOS .DS_Store, Windows Thumbs.db, editor swap files, common cache directories", "Tags adr-kit--v0.1.0, adr-kit--v0.2.0, adr-kit--v0.3.0 exist on the canonical repo, pointing at the same commits as the existing v0.1.0, v0.2.0, v0.3.0 tags", "Tag adr-kit--v0.4.0 exists on the canonical repo, pointing at the v0.4.0 release commit", "v0.4.0 plugin.json version field is bumped accordingly", "GitHub Release v0.4.0 published with notes that explicitly mention the new tag convention and link to CHANGELOG.md", "OTGW-firmware tools/adr-kit/ mirror is in sync with the canonical repo at v0.4.0", "All edits are em-dash-free, English, no emojis"]
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 adr-kit/CHANGELOG.md exists in Keep a Changelog format with at least four version sections (0.1.0, 0.2.0, 0.3.0, 0.4.0)
- [x] #2 adr-kit/.gitignore exists with sensible defaults
- [x] #3 Tags adr-kit--v0.1.0, adr-kit--v0.2.0, adr-kit--v0.3.0 exist on the canonical repo, mirroring the existing v0.1.0/v0.2.0/v0.3.0 tags
- [x] #4 Tag adr-kit--v0.4.0 exists on the canonical repo
- [x] #5 plugin.json version is 0.4.0
- [x] #6 GitHub Release v0.4.0 published with notes mentioning the new tag convention and CHANGELOG
- [x] #7 OTGW-firmware tools/adr-kit/ mirror in sync with v0.4.0
- [x] #8 All edits em-dash-free, English, no emojis
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v0.4.0 published. CHANGELOG.md (Keep a Changelog format, v0.1.0 through v0.4.0), .gitignore, plugin.json bumped to 0.4.0. Backfilled adr-kit--v0.1.0/v0.2.0/v0.3.0 tags on existing commits. Tagged adr-kit--v0.4.0 on the v0.4.0 release commit. GitHub Release v0.4.0 published with --latest. Mirror in OTGW-firmware/tools/adr-kit/ synced (CHANGELOG, .gitignore, plugin.json bump). Both repos consistent.
<!-- SECTION:FINAL_SUMMARY:END -->
