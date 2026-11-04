---
id: TASK-1017
title: Firmware instrumentation + configurable gate caps for load-test
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:20'
updated_date: '2026-07-05 23:06'
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
- [x] #1 New hwm/counter stats exposed via /api/v2/stats + telnet, resettable
- [x] #2 Gate caps settable via -D build-flags without code edits
- [x] #3 evaluate.py green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: added rest_inflight_hwm/webfile_inflight_hwm/tcp_active_pcbs/rest_503/webfile_503 to state.heapdiag (OTGW-firmware.h); incremented on the REST and web-file concurrency gates (restAPI.ino processAPI + webFileGateTryAdmit); sampled tcp_active_pcbs at 1Hz via new platformTcpActivePcbCount() shim in platform_esp32.h (lwIP tcp_active_pcbs walk under LOCK_TCPIP_CORE, confirmed CONFIG_LWIP_TCPIP_CORE_LOCKING=y in sdkconfig). Mirrored min_max_block/min_free_heap/max_loop_gap_ms plus the 5 new counters into /api/v2/device/info JSON (hd_* fields) and the telnet 'D' dump (handleDebug.ino); all 5 new counters reset by telnet 'z' (resetHeapWatermark in helperStuff.ino). AC#2 required no code change: REST_MAX_INFLIGHT/WEB_FILE_MAX_INFLIGHT already had #ifndef/#define override guards from TASK-884/ADR-147. Build: ./build.bat --target esp32-classic green (alpha.329, bumped via bin/bump-prerelease.sh). evaluate.py --quick: 68 passed, 1 pre-existing unrelated warning (boards.h tuning check), 0 failed. Committed 00c21e8c on dev.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 5 new resettable load-test counters (rest_inflight_hwm, webfile_inflight_hwm, tcp_active_pcbs, rest_503, webfile_503) to state.heapdiag, exposed via /api/v2/device/info JSON and telnet D-dump, reset via telnet z. Confirmed REST_MAX_INFLIGHT/WEB_FILE_MAX_INFLIGHT gate caps were already build-flag overridable (no change needed). Build + evaluate.py green. Commit 00c21e8c on dev.
<!-- SECTION:FINAL_SUMMARY:END -->
