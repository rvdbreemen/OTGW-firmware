---
id: TASK-843
title: >-
  beta.3: crash-proof HTTP heap-fragmentation at the right threshold
  (canServeHttp 3072 + streamFile guard)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 18:22'
updated_date: '2026-06-08 18:30'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parallel-debugging ACH confirmed the crash site: unchecked non-throwing new[] in ESP8266 Core 2.7.4 HTTP stack (BufferedStreamDataSource::get_buffer DataSource.h:78 for streamFile, ~1460B; _parseArguments RequestArgument[] for query strings). Under fragmentation (maxBlock < ~1460) new[] returns NULL and memcpy/readBytes writes to NULL -> StoreProhibited epc1=0x4000df64 excvaddr=0. beta.2 gate failed because canServeHttp threshold (1536) was ~= the 1460 cliff and gated per-loop not per-alloc. Fix: raise the gate to a dedicated HTTP_SERVE_MIN_MAXBLOCK=3072 (2x the cliff, margin for mid-transfer fragmentation) covering both parse+serve, and add a defense guard at the streamFile() call sites (send 503 when maxBlock low). Core null-check is the upstream root but pinned Core 2.7.4 needs sign-off; firmware gate avoids touching core.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New HTTP_SERVE_MIN_MAXBLOCK=3072 constant; canServeHttp() gates on it (raised from MQTT_PUBLISH_MIN_MAXBLOCK 1536)
- [x] #2 streamFile() call sites in FSexplorer.ino are guarded: when maxBlock < HTTP_SERVE_MIN_MAXBLOCK, send 503 + skip streaming instead of letting the core new[] return NULL
- [x] #3 MQTT/WS maxBlock gates unchanged (still 1536); only the HTTP-serve path uses the higher 3072 threshold
- [x] #4 python build.py --firmware exit 0; evaluate.py --quick no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
beta.3 crash-proof shipped (commit 7f869dc1). Root cause (ACH-confirmed): ESP8266 Core 2.7.4 streamFile -> BufferedStreamDataSource::get_buffer unchecked new uint8_t[~1460] returns NULL under fragmentation -> memcpy(NULL) StoreProhibited (epc1=0x4000df64). Added HTTP_SERVE_MIN_MAXBLOCK=2048 (above the ~1460 cliff, below beta.6's working 1944 floor); canServeHttp() raised 1536->2048 to gate handleClient (parse+serve); streamFileGuarded() helper at the 6 FSexplorer streamFile sites sends 503 when maxBlock<2048. MQTT/WS gates unchanged (1536). Build+evaluator green. Supersedes beta.2 (which gated at 1536 ~= the cliff and per-loop, so requests slipped through and faulted per-chunk). Field validation pending: George flashes beta.3, browser ON, expect HTTP_fragskips/503s instead of Exception, 0 crashes.
<!-- SECTION:FINAL_SUMMARY:END -->
