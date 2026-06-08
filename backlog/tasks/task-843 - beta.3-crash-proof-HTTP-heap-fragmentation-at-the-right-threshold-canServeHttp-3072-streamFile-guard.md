---
id: TASK-843
title: >-
  beta.3: crash-proof HTTP heap-fragmentation at the right threshold
  (canServeHttp 3072 + streamFile guard)
status: To Do
assignee: []
created_date: '2026-06-08 18:22'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Parallel-debugging ACH confirmed the crash site: unchecked non-throwing new[] in ESP8266 Core 2.7.4 HTTP stack (BufferedStreamDataSource::get_buffer DataSource.h:78 for streamFile, ~1460B; _parseArguments RequestArgument[] for query strings). Under fragmentation (maxBlock < ~1460) new[] returns NULL and memcpy/readBytes writes to NULL -> StoreProhibited epc1=0x4000df64 excvaddr=0. beta.2 gate failed because canServeHttp threshold (1536) was ~= the 1460 cliff and gated per-loop not per-alloc. Fix: raise the gate to a dedicated HTTP_SERVE_MIN_MAXBLOCK=3072 (2x the cliff, margin for mid-transfer fragmentation) covering both parse+serve, and add a defense guard at the streamFile() call sites (send 503 when maxBlock low). Core null-check is the upstream root but pinned Core 2.7.4 needs sign-off; firmware gate avoids touching core.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New HTTP_SERVE_MIN_MAXBLOCK=3072 constant; canServeHttp() gates on it (raised from MQTT_PUBLISH_MIN_MAXBLOCK 1536)
- [ ] #2 streamFile() call sites in FSexplorer.ino are guarded: when maxBlock < HTTP_SERVE_MIN_MAXBLOCK, send 503 + skip streaming instead of letting the core new[] return NULL
- [ ] #3 MQTT/WS maxBlock gates unchanged (still 1536); only the HTTP-serve path uses the higher 3072 threshold
- [ ] #4 python build.py --firmware exit 0; evaluate.py --quick no new failures
<!-- AC:END -->
