---
id: TASK-868
title: >-
  Decide REST AsyncResponseStream initial buffer size on ESP32-S3 (pulled out of
  ADR-139)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-14 23:55'
updated_date: '2026-06-15 04:38'
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
- [x] #1 Decision recorded: keep default 1460 OR adopt a tuned size with a measured justification
- [ ] #2 If adopted, REST_STREAM_BUFFER_SIZE (or equivalent) lands as its own commit with its own prerelease bump, not bundled into an unrelated ADR
- [x] #3 Build green on esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DECISION: Keep the ESPAsyncWebServer default (1460 B); do NOT pass a size to beginResponseStream(). The code already reflects this (restBeginStream -> beginResponseStream(contentType) with no size, restored when REST_STREAM_BUFFER_SIZE 4096 was pulled out of ADR-139's commit 50253650). No code change, no prerelease bump; the current committed state (a29c29c0) is build-green (esp32/esp32-classic/esp32-combo) and evaluate-green (98.6%).

EVIDENCE (3-agent investigation, 2026-06-15):
1. cbuf growth is EXACT-FIT, no geometric factor (WebResponses.cpp:930-932 resizeAdd(needed); esp32 cbuf.cpp:71-73). On esp32 the cbuf is a FreeRTOS RingbufHandle_t; each grow = xRingbufferCreate(newSize) + temp malloc of buffered bytes + copy + free temp + delete old ring. K overflow-writes copy the whole payload K times -> O(n^2), and each step needs a fresh CONTIGUOUS block that can FAIL on a fragmented heap (-> short-write / truncated response).
2. A 4096 initial buffer is worst-of-both-worlds: it does NOT eliminate the O(n^2) growth for the responses that matter (the large ones >4096: settings ~9.4KB, debug ~7.8KB, ot-support/boiler-support up to ~14-20KB pathological), AND it over-allocates a 4096 contiguous ring for EVERY response including the common tiny ones (health 220B, discovery 300B, most <1.5KB).
3. one_live_at_a_time = FALSE: multiple concurrent connections each hold their own cbuf, so a larger initial size is multiplied by in-flight responses, not amortized. (The pulled REST_STREAM_BUFFER_SIZE comment's 'only one cbuf live at a time' was incorrect: the single async task serializes WRITES, but per-connection cbufs coexist while draining.)
4. ADR-089: no heap compaction -> a large contiguous alloc fails silently when maxFreeBlock is low; many small allocs / on-demand growth is the fragmentation-friendly shape. A big pre-alloc is the precise anti-pattern ADR-089 warns against.
5. References: OT-Thing-OTGW32 uses the default (no size). EMS-ESP32 does not pre-size but builds a full ArduinoJson doc in heap, safe only because it has PSRAM (not this board). Project convention (jsonStuff.ino): no ArduinoJson, imperative streaming via small fixed stack buffers + on-demand stream growth; the old sTxBuf coalescing was deliberately removed in favour of letting the async stream coalesce. Keeping the default is consistent with all of these.

AC2 is conditional ('if adopted'): not triggered, since the default is kept (no code change to commit/bump).

RELATED FINDING (out of scope, candidate follow-up): the exact-fit O(n^2) growth + contiguous-realloc-can-fail means large/unbounded responses (settings ~9.4KB; ot-support/boiler-support up to ~14-20KB on pathological bus population) risk a truncated JSON body on a fragmented heap. device/info already gates on an 8192B max-free-block (503 below); settings and the two unbounded otgw endpoints do not. A heap-health gate (or bounding the unbounded endpoints) would close that gap.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Investigated REST AsyncResponseStream buffer sizing on ESP32-S3. Decision: keep the library default (1460), no size argument (already in the code after REST_STREAM_BUFFER_SIZE was pulled from ADR-139). A larger initial buffer was rejected on evidence: exact-fit O(n^2) cbuf growth means 4096 does not help the large responses it targets while over-allocating for the common small ones, per-connection cbufs are not one-live-at-a-time, and a bigger contiguous alloc is the ADR-089 fragmentation anti-pattern. No code change, no bump; committed state is build-green/evaluate-green. Named follow-up: large/unbounded responses risk truncation on a fragmented heap (settings + two unbounded otgw endpoints lack the max-free-block gate device/info has).
<!-- SECTION:FINAL_SUMMARY:END -->
