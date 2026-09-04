---
id: TASK-1117
title: The beta release workflow claims every prerelease came from the dev branch
status: Done
assignee:
  - '@claude'
created_date: '2026-09-03 05:13'
updated_date: '2026-09-04 07:23'
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
- [x] #3 The change is verified against a real published prerelease, not only by reading the workflow
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

2026-09-04: AC #3 verified against a real published prerelease, v1.7.6-beta.1.

CI run 33848388372 completed success in 1m2s. The release is prerelease=true, draft=false, with all nine assets attached.

The live body opens with:

  Beta prerelease **v1.7.6-beta.1**.

  Built and published automatically from the `otgw-1.x.x` branch.

  ## What is in this build
  ### Added
  - **A reboot command on the telnet console.** ...

Both defects are confirmed fixed on a real page rather than in a local simulation. The branch statement is correct and derived: only otgw-1.x.x contained the tag at build time, so the single-match path fired and named it. Under the old code this same page would have read "Published automatically from a tag push on the `dev` branch", which was false. And the page leads with the change summary, pulled from the CHANGELOG [Unreleased] section, instead of the glossary of asset types.

One thing this release surfaced that the task did not anticipate: v1.7.5 had shipped with all its entries still under [Unreleased] and no [1.7.5] section, because the /release skill never rolls one over. Since the fixed workflow now sources the release body from [Unreleased], this beta would have advertised the whole of 1.7.5 as new. Fixed in the same session by closing the 1.7.5 section and opening a fresh one, which is why the body above lists only the four items that actually landed since the release.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The beta release page no longer lies about where the build came from, and it leads with what the build changes.

The generated body stated "Published automatically from a tag push on the `dev` branch" on every beta. That was false on every beta this workflow has ever published: they are all tagged on otgw-1.x.x. It had been corrected by hand once, on v1.7.5-beta.7, which does nothing for the next one. The branch is now derived with git branch -r --contains, named only when exactly one branch matches, and replaced with "from the tagged commit" when it is ambiguous. A missing sentence is a smaller defect than a confident wrong one.

The same assumption sat in the workflow_dispatch ref guard, which accepted only dev or a SHA and defaulted to dev. Re-publishing a stuck 1.x beta with a missing tag would therefore have tagged dev HEAD and shipped 2.0.0 ESP32 code under a 1.7.x version number. otgw-1.x.x is now accepted, the default is removed so the caller must name the line, and an empty ref fails closed. The TASK-656 properties are unchanged: main and arbitrary PR refs are still rejected.

The body previously opened with links and an explanation of what a .ino.bin is. It now opens with the change summary, sourced from the CHANGELOG [Unreleased] section, which the /beta-prerelease skill already keeps current by refusing to ship until the TASK ids in the commits match the ids written under it. RELEASE_NOTES_<version>.md remains preferred where it exists, which is at stable release time. The asset glossary moved into a collapsed block below the substance.

Verified on v1.7.6-beta.1: CI run 33848388372 success, nine assets, prerelease, and a live page reading "Built and published automatically from the `otgw-1.x.x` branch".
<!-- SECTION:FINAL_SUMMARY:END -->
