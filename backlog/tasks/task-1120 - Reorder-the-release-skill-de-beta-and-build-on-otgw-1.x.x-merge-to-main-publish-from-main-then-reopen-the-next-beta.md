---
id: TASK-1120
title: >-
  Reorder the release skill: de-beta and build on otgw-1.x.x, merge to main,
  publish from main, then reopen the next beta
status: To Do
assignee: []
created_date: '2026-09-03 19:36'
labels: []
dependencies: []
ordinal: 210000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1118 and TASK-1119, correcting the order after the maintainer described the intended flow end to end.\n\nTASK-1118 tagged and published from otgw-1.x.x and brought main forward afterwards. The maintainer wants the older shape instead, with dev replaced by otgw-1.x.x: come out of beta, merge into main, build and publish from main, then bring the released version back to otgw-1.x.x and reopen the next beta cycle.\n\nOne constraint shapes how far that can be taken literally. Every build rewrites version.h, data/version.hash and the version banner in about 24 files. If the release build runs on main, those commits exist only on main, main stops being a strict subset of otgw-1.x.x, and the Phase 3 guard trips at the next release. So the de-beta edit and the release build both belong on otgw-1.x.x; main then receives them by fast-forward and is tagged there. That satisfies both the requested order and the one-way invariant, and it matches the rule that a tag should only be placed once the target branch demonstrably contains the release content.\n\nThe reopen step also needs a decision rather than a hardcoded rule. Release history shows both shapes: 1.7.0 to 1.7.1 to 1.7.2 to 1.7.4 are patch bumps, while 1.6.1 to 1.7.0 is a minor bump. Hardcoding either would be wrong for half the releases.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The de-beta edit and the release build both happen on otgw-1.x.x, so main never carries a commit the release branch lacks
- [ ] #2 main receives the release by merge, and the tag and GitHub release target main
- [ ] #3 After publication the released state is confirmed present on otgw-1.x.x before the next cycle is opened
- [ ] #4 The reopen step asks whether the next cycle is a patch or a minor instead of hardcoding one, and re-enables the prerelease tag
- [ ] #5 The ordering constraint is written down: the sync must precede the bump, because bump-prerelease.sh cannot reopen a prerelease from a clean stable build
<!-- AC:END -->
