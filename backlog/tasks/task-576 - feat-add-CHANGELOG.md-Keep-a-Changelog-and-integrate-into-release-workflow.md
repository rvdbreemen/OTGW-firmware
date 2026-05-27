---
id: TASK-576
title: 'feat: add CHANGELOG.md (Keep a Changelog) and integrate into release workflow'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 22:55'
updated_date: '2026-05-08 21:34'
labels:
  - docs
  - release
  - changelog
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement a CHANGELOG.md following https://keepachangelog.com/en/1.1.0/ format.\n\nTwo parts:\n1. Create historical CHANGELOG.md with entries for every release from v1.0.0 to v1.5.0 (current beta), sourced from release notes, git log, and existing docs/releases/ files.\n2. Update /update-docs skill (SKILL.md) to include CHANGELOG.md maintenance as a mandatory AC in the release phase (3C-6).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 CHANGELOG.md created at repo root following keepachangelog.com 1.1.0 format with entries for every release from v1.0.0 to v1.5.0
- [x] #2 Each version entry has correct sections: Added, Changed, Fixed, Removed, Deprecated, Security (only sections with content)
- [x] #3 Unreleased section present at top for tracking upcoming changes
- [x] #4 /update-docs skill updated: CHANGELOG.md update added as AC 3C-6 in the release phase with clear instructions
- [x] #5 CHANGELOG.md committed and pushed to origin/dev
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
CHANGELOG.md already existed from the v1.5.0 release prep (commit 0719e086). All ACs were satisfied:\n\n- AC#1-3: CHANGELOG.md at repo root with keepachangelog 1.1.0 format, entries v1.0.0 to v1.5.0, Unreleased section at top.\n- AC#4: update-docs skill already updated (commit 3bde46d4) with CHANGELOG.md as AC 3C-6.\n- AC#5: CHANGELOG.md committed and pushed.\n\nAdditional work this session: updated Unreleased section with 1.5.1-beta.1 through beta.3 changes (JIT discovery, TASK-589, TASK-590).\nPushed: origin/dev 7612870a.
<!-- SECTION:FINAL_SUMMARY:END -->
