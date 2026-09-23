---
id: TASK-1162
title: Static file downloads are sometimes served truncated or not at all
status: To Do
assignee: []
created_date: '2026-09-23 21:39'
labels:
  - web
  - bug
dependencies: []
priority: medium
ordinal: 293000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen 2026-09-23 on the OTGW32 bench (alpha.376): three back-to-back GETs of /settings.ini (5752 B on LittleFS) returned 4172 B with HTTP 200, then no response (curl 000), then the full 5752 B. A truncated 200 is worse than an error: it looks like a valid file. Relevant to anyone saving settings.ini before a flash, and to web UI assets. Possibly the ADR-147 file-serve gate or chunked streaming under load; not investigated yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: repeated GETs of a multi-KB static file on the bench, record size and status per request
- [ ] #2 Root cause identified
- [ ] #3 A static file is either served complete or fails with an error status; never a short 200
<!-- AC:END -->
