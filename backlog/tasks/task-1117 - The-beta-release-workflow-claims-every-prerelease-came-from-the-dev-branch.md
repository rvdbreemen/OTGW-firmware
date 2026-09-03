---
id: TASK-1117
title: The beta release workflow claims every prerelease came from the dev branch
status: To Do
assignee: []
created_date: '2026-09-03 05:13'
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
- [ ] #1 The generated release body no longer names a branch the tag did not come from
- [ ] #2 A published beta release page leads with what the build changes rather than with a glossary of asset types
- [ ] #3 The change is verified against a real published prerelease, not only by reading the workflow
<!-- AC:END -->
