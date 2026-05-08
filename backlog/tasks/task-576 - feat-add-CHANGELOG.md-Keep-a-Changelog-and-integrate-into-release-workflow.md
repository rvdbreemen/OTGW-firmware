---
id: TASK-576
title: 'feat: add CHANGELOG.md (Keep a Changelog) and integrate into release workflow'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 22:55'
updated_date: '2026-05-08 21:33'
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
