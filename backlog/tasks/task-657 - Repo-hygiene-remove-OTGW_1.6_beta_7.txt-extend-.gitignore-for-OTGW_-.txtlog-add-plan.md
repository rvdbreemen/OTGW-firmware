---
id: TASK-657
title: >-
  Repo hygiene: remove OTGW_1.6_beta_7.txt + extend .gitignore for
  OTGW_*.{txt,log} + add /plan/
status: To Do
assignee: []
created_date: '2026-05-22 05:52'
labels:
  - repo-hygiene
dependencies: []
references:
  - .gitignore
  - OTGW_1.6_beta_7.txt
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
An 830 KB field-test debug log (OTGW_1.6_beta_7.txt, commit 67e63d97 'Add files via upload') is still in the working tree. .gitignore only catches /logfile.* and /.codex — not the OTGW_*.txt pattern that TASK-635 / commit 4cca4d07 was trying to defend against. The plan/ directory was also uploaded via web UI on 2026-05-20 (~874 lines including a 17k-line Beta_11 log) then deleted 7 minutes later — packfile bloat is permanent unless rewritten, but /plan/ should be ignored to prevent recurrence.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 git rm OTGW_1.6_beta_7.txt; commit removes the file from the working tree
- [ ] #2 .gitignore extended with /OTGW_*.txt and /OTGW_*.log (anchored to repo root)
- [ ] #3 .gitignore extended with /plan/ to prevent future web-UI bulk uploads
- [ ] #4 No previously-tracked file is shadowed by the new ignore patterns (git ls-files | grep -E 'OTGW_.*\.(txt|log)' returns empty before commit)
- [ ] #5 Commit cites TASK-635 in subject for traceability
<!-- AC:END -->
