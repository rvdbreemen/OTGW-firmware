---
id: TASK-315
title: >-
  [PERF-L1] Replace String origin allocation in sendCorsOriginHeader with
  strlcpy
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:23'
updated_date: '2026-04-19 06:21'
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
- [x] #1 sendCorsOriginHeader uses static char buffer + strlcpy, no String
- [x] #2 No heap allocation during normal API requests
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
restAPI.ino sendCorsOriginHeader: replaced String origin = httpServer.header('Origin') with a static char[128] + strlcpy. httpServer.header returns const String& (verified against ESP8266WebServer-impl.h), so the copy into buffer is truly allocation-free. Comment documents the rationale inline.
<!-- SECTION:FINAL_SUMMARY:END -->
