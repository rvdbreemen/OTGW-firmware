---
id: TASK-303
title: >-
  [PERF-M1] Stream weatherFetch response instead of String full-buffer
  allocation
status: To Do
assignee: []
created_date: '2026-04-18 19:20'
labels:
  - performance
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SATweather.ino:148 uses String payload = http.getString(), allocating 2-4 KB heap every 15 min. Comment calls it one-off but it runs continuously. Violates ADR-004 on hot path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Replace with fixed static buffer + http.getStreamPtr() or bounded readBytes
- [ ] #2 Parse floats/arrays on the fly with strstr_P over chunks
- [ ] #3 Heap allocation during weather fetch confirmed under 256 bytes
<!-- AC:END -->
