---
id: TASK-1096
title: FSexplorer upload ignores current subdirectory
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 17:00'
updated_date: '2026-09-01 17:01'
labels: []
dependencies: []
ordinal: 194000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Uploading a file from a subdirectory in FSexplorer always writes it to the LittleFS root. The XHR submit handler posts to a hardcoded /upload URL and drops the ?path= query the form action carries, so the server-side handler never sees the target directory. Reported on 1.7.4+b77304b while uploading PIC hex files into /pic16f1847.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Uploading from a subdirectory stores the file in that subdirectory
- [ ] #2 Uploading from root still stores the file in root
- [ ] #3 Filesystem image builds and evaluator stays green
<!-- AC:END -->
