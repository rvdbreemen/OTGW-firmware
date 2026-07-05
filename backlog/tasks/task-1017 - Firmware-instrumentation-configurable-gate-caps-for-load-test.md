---
id: TASK-1017
title: Firmware instrumentation + configurable gate caps for load-test
status: To Do
assignee: []
created_date: '2026-07-05 22:20'
labels: []
dependencies: []
ordinal: 228000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Add resettable high-watermark stats: rest_inflight_hwm, webfile_inflight_hwm, tcp_active_pcbs, rest_503/webfile_503, min_max_block, min_free_heap, max_loop_gap_ms. Expose via /api/v2/stats JSON + telnet dump, reset on telnet 'z'. Drive REST_MAX_INFLIGHT + WEB_FILE_MAX_INFLIGHT from build-flags per arm.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New hwm/counter stats exposed via /api/v2/stats + telnet, resettable
- [ ] #2 Gate caps settable via -D build-flags without code edits
- [ ] #3 evaluate.py green
<!-- AC:END -->
