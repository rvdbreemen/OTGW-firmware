---
id: TASK-2920
title: >-
  flash_otgw.sh is stored in git with CRLF line endings although .gitattributes
  says eol=lf
status: To Do
assignee: []
created_date: '2026-11-19T16:08:00Z'
labels:
  - tools
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1058000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-11-19 (TASK-2028): git show origin/dev:flash_otgw.sh has a CR on 499 of 499 lines (after TASK-2028: 586 of 586), while git check-attr gives eol: lf, text: set. The blob was committed with CRLF, so the attribute does not convert it. No user impact today: the working copy is LF and build.py packs the script from it; the flash zip of 2.0.0-beta.391+40f511a holds flash_otgw.sh with 0 CR. Risk: anyone taking the script straight from git (raw GitHub download, a Linux checkout that keeps the blob bytes) can get a CRLF script that fails with $'\r': command not found.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 git ls-files --eol shows flash_otgw.sh as i/lf (normalised blob), and the other *.sh files checked the same way
- [ ] #2 A check (test or evaluate.py) fails when a tracked *.sh blob contains CR, red on today's blob
<!-- AC:END -->
