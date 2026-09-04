---
id: TASK-1117
title: The beta release workflow claims every prerelease came from the dev branch
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-03 05:13'
updated_date: '2026-09-04 06:18'
labels: []
dependencies: []
ordinal: 207000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
beta-prerelease.yml writes the GitHub release body itself, and that body contains the line "Published automatically from a tag push on the dev branch". Beta prereleases on this line are tagged on otgw-1.x.x, never on dev, so the statement is wrong on every beta this workflow has ever published. It was noticed on v1.7.5-beta.7 and corrected there by hand with gh release edit, which does not help the next one.\n\nThe rest of the generated body is generic scaffolding: links to README.md and CHANGELOG.md, a glossary of what a .ino.bin and a .littlefs.bin are, and a boilerplate field-testing paragraph. None of it tells a tester what the build actually fixes, which is the one thing they open the page for. The maintainer asked for the opposite: a short concrete summary per defect.\n\nTwo separable changes, and the second is a judgement call worth making deliberately: correct the branch statement so it stops being false, and decide whether CI should keep authoring a body at all rather than leaving it to the release author.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The generated release body no longer names a branch the tag did not come from
- [x] #2 A published beta release page leads with what the build changes rather than with a glossary of asset types
- [ ] #3 The change is verified against a real published prerelease, not only by reading the workflow
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix the false branch statement: derive the branch that actually contains the tag with git branch -r --contains, name it only when exactly one branch matches, and omit the clause otherwise. Never hardcode a branch name.
2. Fix the same dev assumption in the workflow_dispatch ref guard, which today accepts only dev or a SHA and defaults to dev. On this line a stuck 1.x beta would be tagged at dev HEAD, publishing 2.0.0 code under a 1.7.x version. Allow otgw-1.x.x, drop the default, and fail closed when a missing tag is dispatched with no ref.
3. Reorder the body so it leads with substance: pull the CHANGELOG [Unreleased] section at the tagged commit, which the beta-prerelease skill already keeps current, and fall back to the RELEASE_NOTES digest. Demote the asset glossary below it.
4. Verify by executing the composition block locally against a real tagged commit, then against a real published prerelease when the next beta ships.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-04: implemented and verified by execution.

AC #1: the branch is derived (git branch -r --contains the tag), named only when exactly one branch matches, otherwise the body says "from the tagged commit". Unit-tested across one match (otgw-1.x.x, dev), two matches, and zero. The workflow can no longer name a branch the tag did not come from.

AC #2: the body now opens with the change summary. RELEASE_NOTES_<version>.md stays preferred, but it does not exist during a beta cycle, so the CHANGELOG [Unreleased] section is the fallback. Executed the real composition step against a worktree at tag v1.7.5-beta.8: it found no notes file, fell back to the CHANGELOG, and produced a body opening on the Security section instead of the asset glossary. The glossary is now a collapsed block below Flashing and Reporting.

Also fixed, same root assumption: the workflow_dispatch ref guard accepted only dev or a SHA and defaulted to dev, so re-publishing a stuck 1.x beta with a missing tag would have tagged dev HEAD and shipped 2.0.0 ESP32 code under a 1.7.x version. otgw-1.x.x is now allowed, the default is gone, and an empty ref fails closed. TASK-656 properties preserved: main and PR refs still rejected, verified by test.

AC #3 open: it requires a real published prerelease. The next beta on this line will exercise it.
<!-- SECTION:NOTES:END -->
