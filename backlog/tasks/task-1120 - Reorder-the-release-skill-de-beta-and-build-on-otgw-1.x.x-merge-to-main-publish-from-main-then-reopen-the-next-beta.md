---
id: TASK-1120
title: >-
  Reorder the release skill: de-beta and build on otgw-1.x.x, merge to main,
  publish from main, then reopen the next beta
status: Done
assignee: []
created_date: '2026-09-03 19:36'
updated_date: '2026-09-03 19:40'
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
- [x] #1 The de-beta edit and the release build both happen on otgw-1.x.x, so main never carries a commit the release branch lacks
- [x] #2 main receives the release by merge, and the tag and GitHub release target main
- [x] #3 After publication the released state is confirmed present on otgw-1.x.x before the next cycle is opened
- [x] #4 The reopen step asks whether the next cycle is a patch or a minor instead of hardcoding one, and re-enables the prerelease tag
- [x] #5 The ordering constraint is written down: the sync must precede the bump, because bump-prerelease.sh cannot reopen a prerelease from a clean stable build
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 5 now runs: de-beta edit, release build, commit the full sweep, push, THEN merge otgw-1.x.x into main in a throwaway worktree, then create and publish the draft with --target main. Phase 6 no longer merges anything; it proves main and otgw-1.x.x are identical and then reopens the cycle.

The reopen step asks patch-or-minor instead of hardcoding. The old text said "increment patch", which would have been wrong at 1.6.1 to 1.7.0; hardcoding minor would be wrong at 1.7.0 through 1.7.4. Both shapes appear in the published history, so it is a per-release decision and the skill puts it to the maintainer.

Verified after editing: no occurrence of the old ref-push form, no "increment patch", no "sync dev" heading. Every branch operation targets otgw-1.x.x except the release itself, which targets main by design.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reordered /release to the flow the maintainer specified: out of beta, merge to main, publish from main, then reopen the next beta on otgw-1.x.x.

The substantive constraint is why the build stays on otgw-1.x.x rather than moving to main with the tag. Every build rewrites version.h, data/version.hash and the banner in about 24 files; running it on main would leave those commits only on main, so main would stop being a strict subset of the release branch and the Phase 3 guard would trip at the next release. Building on otgw-1.x.x and letting main fast-forward to the finished result satisfies the requested order, keeps the release sync one-way, and still tags a branch that demonstrably contains the release.

The reopen step now asks whether the next cycle is a patch or a minor. The previous text hardcoded patch, which the 1.6.1 to 1.7.0 step contradicts, and hardcoding minor would contradict 1.7.0 through 1.7.4. It is a per-release call that propagates into every beta tag of the cycle.
<!-- SECTION:FINAL_SUMMARY:END -->
