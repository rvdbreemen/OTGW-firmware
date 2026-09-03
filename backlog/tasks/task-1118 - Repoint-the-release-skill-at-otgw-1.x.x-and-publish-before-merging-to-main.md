---
id: TASK-1118
title: Repoint the release skill at otgw-1.x.x and publish before merging to main
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 19:21'
updated_date: '2026-09-03 19:26'
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
- [x] #1 The skill names otgw-1.x.x as the release source branch and never merges dev into main
- [x] #2 The release is published from otgw-1.x.x, and main is updated by merging afterwards
- [x] #3 The skill carries a worktree preflight so a tool call cannot operate on the wrong branch, matching the beta-prerelease skill
- [x] #4 The next-cycle prerelease bump targets otgw-1.x.x, and the sync-back into it is ordered before that bump
- [x] #5 The skill states explicitly that dev is the 2.0.0 line and is out of scope for this skill
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified after editing: every remaining occurrence of "dev" in the file is a warning that it is out of scope. All branch operations target otgw-1.x.x, the release is created with --target otgw-1.x.x, and main is advanced afterwards.

One improvement beyond the stated scope, made because the obvious version was risky: bringing main forward uses `git push origin otgw-1.x.x:main` rather than checkout, merge, push, checkout back. That never moves the release worktree off its branch (the exact hazard the new preflight guards against), and git refuses a non-fast-forward push by default, so a diverged main fails loudly instead of gaining a silent merge commit. The skill says not to reach for --force there and explains why.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Repointed /release from dev to otgw-1.x.x and inverted the merge order.

The skill predated the 2026-06-20 branch-model change and still treated dev as the release source. Since dev now carries the 2.0.0 ESP32/SAT line, following it literally would have merged 1334 alpha commits onto main and published them as the stable release that ESP8266 users pull through the flash scripts, irreversibly, because published tags are immutable. It was caught before execution.

The flow is now: work on otgw-1.x.x, publish from there, then fast-forward main to match. Phase 3 stopped merging and became a guard that proves main is strictly behind. Phase 0 gained a worktree preflight. Phase 6 advances main with a ref push instead of a checkout, and the next-cycle bump moved to otgw-1.x.x, explicitly ordered after the sync because bump-prerelease.sh cannot reopen a prerelease from a stable build.
<!-- SECTION:FINAL_SUMMARY:END -->
