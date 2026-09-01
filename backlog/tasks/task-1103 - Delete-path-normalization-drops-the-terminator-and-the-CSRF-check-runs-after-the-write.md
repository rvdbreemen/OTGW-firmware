---
id: TASK-1103
title: >-
  Delete path normalization drops the terminator, and the CSRF check runs after
  the write
status: To Do
assignee: []
created_date: '2026-09-01 18:36'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 200000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two defects around the order and bounds of path handling. The delete handler prefixes a missing leading slash with memmove after strnlen caps the length at the buffer size minus two, so an argument of 63 or more characters loses its NUL terminator and the compare runs off the end of the buffer. Separately the upload path writes the file to LittleFS before the same-origin check runs, so that check can reject the response but never prevents the write: a cross-origin post overwrites index.html and only then gets refused. Found by the FSexplorer audit of 2026-09-01, findings 7 and 11 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A delete argument at or beyond the buffer length is rejected or truncated safely, with the terminator intact
- [ ] #2 A request that fails the same-origin check does not write to the filesystem
<!-- AC:END -->
