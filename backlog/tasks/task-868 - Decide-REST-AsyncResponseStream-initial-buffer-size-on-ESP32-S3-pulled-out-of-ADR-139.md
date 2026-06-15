---
id: TASK-868
title: >-
  Decide REST AsyncResponseStream initial buffer size on ESP32-S3 (pulled out of
  ADR-139)
status: To Do
assignee: []
created_date: '2026-06-14 23:55'
labels: []
dependencies: []
ordinal: 84000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ADR-139 web-serving commit (50253650) bundled an out-of-scope change: raising the REST AsyncResponseStream initial cbuf from the library default (1460 = 1 TCP MSS) to 4096 via REST_STREAM_BUFFER_SIZE in webServerCompat.h restBeginStream(). The rationale is sound (ESP32-S3 has 327 KB RAM; our big JSON endpoints device/info/otmonitor/BLE-roster serialize in one alloc instead of 2-3 realloc growth steps; this firmware is heap-fragmentation sensitive per ADR-089). But it was never part of ADR-139's decision, so it was pulled back out (reverted to beginResponseStream(contentType)) when ADR-139 was accepted, to be decided on its own merits. Re-evaluate: pick a value (or keep library default), justify against ADR-089 heap tiers, measure realloc churn on the large endpoints, and land it as its own commit if adopted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decision recorded: keep default 1460 OR adopt a tuned size with a measured justification
- [ ] #2 If adopted, REST_STREAM_BUFFER_SIZE (or equivalent) lands as its own commit with its own prerelease bump, not bundled into an unrelated ADR
- [ ] #3 Build green on esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->
