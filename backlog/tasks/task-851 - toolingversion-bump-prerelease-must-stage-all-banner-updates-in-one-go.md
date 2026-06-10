---
id: TASK-851
title: 'tooling(version): bump-prerelease must stage all banner updates in one go'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-10 19:51'
updated_date: '2026-06-10 19:54'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
bump-prerelease.sh calls autoinc-semver.py which updates version.h AND all source-file version banners, but the documented commit step only stages version.h + data/version.hash. Banner updates in ~43 files drift uncommitted (observed: banners at alpha.165 while version.h at alpha.170). Fix: autoinc-semver.py gains --print-updated (lists every file it wrote), bump-prerelease.sh stages them all, CLAUDE.md bump instructions updated. Also sync the pending alpha.171 banner churn in one commit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 autoinc-semver.py --print-updated prints relpath of every file it modified (version.h, data/version.hash, banner files)
- [x] #2 bump-prerelease.sh runs with --update-all and git-adds every file the script touched
- [x] #3 CLAUDE.md versioning section no longer instructs staging only version.h + version.hash
- [x] #4 pending alpha.171 banner drift committed in a single sync commit
<!-- AC:END -->
