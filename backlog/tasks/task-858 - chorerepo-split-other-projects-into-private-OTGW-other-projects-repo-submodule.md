---
id: TASK-858
title: >-
  chore(repo): split other-projects into private OTGW-other-projects repo +
  submodule
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-11 16:50'
updated_date: '2026-06-11 17:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reference projects/documentation under other-projects/ must stay private to the maintainer. Create private GitHub repo OTGW-other-projects holding that content, wire it into OTGW-firmware as a git submodule at other-projects/, and gitignore the path defensively. Main repo stays single entry point.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Private repo OTGW-other-projects exists on GitHub with the reference content
- [x] #2 other-projects/ is a submodule in the 2.0.0 branch (.gitmodules)
- [x] #3 Tracked other-projects files removed from the main repo index
- [x] #4 .gitignore covers other-projects for the non-submodule case
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Private repo rvdbreemen/OTGW-other-projects created (main, snapshot commit 90cd6179, 67.6 MiB pack, no file >40MB). Built in-place from the 2.0.0 worktree content; nested otgwmcu/.git flattened. Submodule registered at other-projects/ (reused in-place repo, no re-clone). README.md de-tracked from main repo (lives in private repo). .gitignore: defensive other-projects/ entry. Dev-branch wiring follows via merge/port.
<!-- SECTION:NOTES:END -->
