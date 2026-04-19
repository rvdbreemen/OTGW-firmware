---
id: TASK-315
title: >-
  [PERF-L1] Replace String origin allocation in sendCorsOriginHeader with
  strlcpy
status: To Do
assignee: []
created_date: '2026-04-18 19:23'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
restAPI.ino:44-48 allocates String per CORS call (~16-64 bytes). Fragmentation over time. Mirror the isSameOriginRequest pattern (static originBuf[128] + strlcpy).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 sendCorsOriginHeader uses static char buffer + strlcpy, no String
- [ ] #2 No heap allocation during normal API requests
<!-- AC:END -->
