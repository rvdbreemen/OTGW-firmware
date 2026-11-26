---
id: TASK-3098
title: >-
  commit-msg check: docs(adr)/docs(skill) subject naming Robert or Accepted must
  not carry a time later than the author date (retro proposal 2c)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:14:00Z'
labels: []
dependencies: []
ordinal: 1163000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert adopted retro proposal 2c (DM 1575388608725000225 A). commit-msg calls scripts/check_commit_subject_time.py for subjects starting docs(adr) or docs(skill) that contain Robert or Accepted: fail when the subject has YYYY-MM-DD HH:MM or YYYY-MM-DD ~HH:MM later than the AUTHOR date with its UTC offset (not the committer date, a rebase rewrites it). Past exact times pass; planned slots like 08:00 beta do not trigger.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Red on the subject of 7e127ff84 (authored 00:44, says ~01:10); green on 62b60aa98 and 9bec44df4
- [ ] #2 No trigger on a planned slot or on other prefixes; the real commit-msg hook runs it
<!-- AC:END -->
