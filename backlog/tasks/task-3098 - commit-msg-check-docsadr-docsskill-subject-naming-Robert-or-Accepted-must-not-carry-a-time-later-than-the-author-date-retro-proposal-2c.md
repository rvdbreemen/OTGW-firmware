---
id: TASK-3098
title: >-
  commit-msg check: docs(adr)/docs(skill) subject naming Robert or Accepted must
  not carry a time later than the author date (retro proposal 2c)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-26T06:14:00Z'
updated_date: '2026-11-26T07:08:00Z'
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
- [x] #1 Red on the subject of 7e127ff84 (authored 00:44, says ~01:10); green on 62b60aa98 and 9bec44df4
- [x] #2 No trigger on a planned slot or on other prefixes; the real commit-msg hook runs it
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done 2026-11-26 (otgw-webui), pushed aaf69458c: check_commit_subject_time.py called by commit-msg; red on the subject of 7e127ff84, green on 62b60aa98 and 9bec44df4; 8 tests incl. the real hook, 5 mutants caught.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 red on the subject of 7e127ff84 (authored 00:44, says ~01:10), green on 62b60aa98 and 9bec44df4 (06:01, authored 06:05); tests/test_commit_subject_time.py 8/8, 5 mutants caught; pushed aaf69458c. AC2 a planned slot (08:00 beta), other prefixes and a missing name do not trigger; the real commit-msg hook runs the script (test with the real hook). Host-verified.
<!-- SECTION:FINAL_SUMMARY:END -->
