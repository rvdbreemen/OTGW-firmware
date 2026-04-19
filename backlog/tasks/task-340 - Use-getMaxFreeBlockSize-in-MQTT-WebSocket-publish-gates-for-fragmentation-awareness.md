---
id: TASK-340
title: >-
  Use getMaxFreeBlockSize in MQTT/WebSocket publish gates for fragmentation
  awareness
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-19 21:04'
updated_date: '2026-04-19 21:13'
labels:
  - mqtt
  - heap
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extend canPublishMQTT() and canSendWebSocket() to also consider ESP.getMaxFreeBlockSize() instead of only ESP.getFreeHeap(). Fragmented heap can show freeHeap=8KB while maxFreeBlock=2KB, which makes the next allocation fail even though the current gate thinks we are healthy.

**Background**
ESP8266 umm_malloc uses a free-list with no compaction. Over time heap fragments, especially with variable-size MQTT buffers coming/going. A publish of ~1.2KB JSON can fail if maxFreeBlock < 1.2KB even though total free is higher.

Crashevans log shows prefix format `(freeHeap|maxFreeBlock)`: e.g. `(12584|10408)` = 12KB free but only 10KB max contiguous. Ratio drops to ~30% after a few hours uptime on busy networks.

**Considerations**
- Pro: catches the actual allocation-failure risk, not a proxy metric
- Pro: existing HEAP_CRITICAL/WARNING/LOW thresholds can be reused for maxFreeBlock check
- Con: getMaxFreeBlockSize() walks the entire free list — not free (see Debug.h:113 comment). Call site runs per-publish, so could add measurable CPU if called on every message.
- Con: gate semantics get murkier — if freeHeap=healthy but maxBlock=low, is that "ok to send small" or "block everything"?
- Mitigation: only check maxFreeBlock in getHeapHealth() when freeHeap is already in LOW/WARNING — skip the walk in the common healthy path.

**Design choice**
Introduce `getHeapFragmentation()` returning an estimate (1 - maxBlock/freeHeap). In getHeapHealth(), promote the level by one tier when fragmentation exceeds 50% AND freeHeap is already in LOW. This is cheap in the common case and catches the real risk.

**Impact estimate**
Measured primarily on long-uptime setups (>24h) where fragmentation accumulates. Prevents the silent allocation-failure OOM that the current gate does not detect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 getHeapHealth() considers getMaxFreeBlockSize() when level is already LOW or below
- [ ] #2 Fragmentation factor documented (maxBlock/freeHeap ratio threshold)
- [ ] #3 Perf check: heap health check must stay below 100us in the common healthy path (skip walk)
- [ ] #4 Debug diagnostics: logHeapStats() already shows both values — no log format change needed
- [ ] #5 Build passes for esp8266 environment
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. In src/OTGW-firmware/helperStuff.ino, extend getHeapHealth() so it also considers getMaxFreeBlockSize() when freeHeap is already in LOW tier — fragmentation can silently make the next alloc fail even when freeHeap looks ok
2. Only walk the free-list in the non-healthy path to keep the common case cheap
3. Introduce a small local helper getHeapFragmentation() for observability (returns percentage)
4. Keep logHeapStats() output format unchanged (already logs both values)
5. Build esp8266 and sanity-check via telnet debug that fragmentation is reported sensibly
6. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extended getHeapHealth() in helperStuff.ino to consult getMaxFreeBlockSize() when freeHeap is in LOW tier. If maxBlock < HEAP_FRAG_PROMOTE_MAXBLOCK (2048 bytes) we promote to WARNING. Kept the HEALTHY fast-path free of the free-list walk. Added getHeapFragmentation() observability helper (returns percent 0..100).
<!-- SECTION:NOTES:END -->
