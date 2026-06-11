---
id: TASK-860
title: 'chore(repo): port other-projects private submodule to dev (TASK-858 sibling)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-11 19:59'
updated_date: '2026-06-11 20:02'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of 2.0.0 TASK-858/b02dbab7: other-projects/ content lives in private repo rvdbreemen/OTGW-other-projects; wire it as a git submodule on dev and replace the plain gitignore entries with the defensive form.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 other-projects/ is a submodule on dev pointing at OTGW-other-projects main
- [x] #2 Local content converted in-place to a checkout of the private repo (verified against GitHub)
- [x] #3 .gitignore carries the defensive other-projects/ entry
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Dev port of TASK-858: other-projects/ converted in-place to a checkout of rvdbreemen/OTGW-other-projects (snapshot 90cd6179, fetched from GitHub - zero content diffs, remote copy verified), registered as submodule, defensive .gitignore entry.
<!-- SECTION:FINAL_SUMMARY:END -->
