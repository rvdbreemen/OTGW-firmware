---
id: TASK-1118
title: Repoint the release skill at otgw-1.x.x and publish before merging to main
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-03 19:21'
updated_date: '2026-09-03 19:22'
labels: []
dependencies: []
ordinal: 208000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The /release skill predates the branch-model change of 2026-06-20 and still hardcodes dev as the release source. Phase 0 does git checkout dev, Phase 2 pushes to dev, Phase 3 merges dev into main, and Phase 6 bumps dev for the next cycle.\n\nUnder the current model dev is the 2.0.0 ESP32 alpha line. Following the skill literally would merge 1334 commits of 2.0.0 alpha onto main and publish them as a stable release that ESP8266 users pull through the flash scripts. A published tag is immutable, so that cannot be undone, only rolled forward. This was caught before execution during a /release invocation on 2026-09-03.\n\nThe maintainer set the intended shape: work on otgw-1.x.x, publish from there, then merge back to main so main reflects the latest release. That also inverts the current order, which merges to main first and tags there.\n\nSecondary correctness point that follows from the same change: the sync-back merge into otgw-1.x.x must precede the next prerelease bump, because bump-prerelease.sh cannot reopen a prerelease from a stable build.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The skill names otgw-1.x.x as the release source branch and never merges dev into main
- [ ] #2 The release is published from otgw-1.x.x, and main is updated by merging afterwards
- [ ] #3 The skill carries a worktree preflight so a tool call cannot operate on the wrong branch, matching the beta-prerelease skill
- [ ] #4 The next-cycle prerelease bump targets otgw-1.x.x, and the sync-back into it is ordered before that bump
- [ ] #5 The skill states explicitly that dev is the 2.0.0 line and is out of scope for this skill
<!-- AC:END -->
