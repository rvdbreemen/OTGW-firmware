---
id: TASK-235
title: >-
  SAT: Reduce SATWindowRecord ring buffer size on ESP8266 to save ~720 bytes
  SRAM
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 07:03'
updated_date: '2026-04-09 10:25'
labels:
  - sat
  - esp8266
  - memory
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SATWindowRecord[60] ring buffer consumes 1,440 bytes of static SRAM (60 x 24 bytes). On ESP8266 with only ~40KB DRAM total and ~20-25KB free heap at runtime, this is significant (~6% of total DRAM). On ESP32 with 520KB SRAM the 60-slot window is fine. Use platform-conditional compilation to use 30 slots on ESP8266 (720 bytes) and 60 slots on ESP32 (1,440 bytes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Define SAT_WINDOW_SIZE as 30 on ESP8266 and 60 on ESP32 using platform guards
- [x] #2 SATWindowRecord ring buffer uses SAT_WINDOW_SIZE slots instead of hardcoded 60
- [x] #3 satGetWindow4hStats() works correctly with reduced window (30 x 2-min samples = 60 min coverage minimum)
- [x] #4 No change in behavior on ESP32
- [x] #5 Verified: no stack overflow in insertion-sort with SAT_WINDOW_SIZE=30 (local copy max 30 floats)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SATcycles.ino to understand SAT_WIN4H_SIZE=60 usage
2. Replace static const uint8_t SAT_WIN4H_SIZE=60 with #if ESP8266 / 30 / #else / 60 / #endif define
3. Update comment near declaration to reflect platform-conditional sizing
4. All other uses (ring buffer, modulo, local deltas[]) already reference SAT_WIN4H_SIZE — no further changes needed
5. Verify local deltas[] on stack: 30*4=120 bytes on ESP8266, well within 4KB CONT stack
6. Commit
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reduced SAT 4-hour cycle window ring buffer from 60 to 30 slots on ESP8266, saving 720 bytes of static SRAM.

Changes:
- Replaced `static const uint8_t SAT_WIN4H_SIZE = 60` with a #if defined(ESP8266) / #else / #endif preprocessor block defining SAT_WIN4H_SIZE as 30 (ESP8266) or 60 (ESP32).
- Updated comments to document the per-platform sizes and stack usage.
- All other usages (_win4h[] declaration, ring buffer modulo, loop bounds, local deltas[] array in satGetWindow4hStats) already referenced SAT_WIN4H_SIZE - no further changes required.

Stack impact: local deltas[] in satGetWindow4hStats() is 30*4=120 bytes on ESP8266, well within the 4KB CONT stack budget.
ESP32 behavior is completely unchanged (60 slots).
No logic changes - pure size reduction.
<!-- SECTION:FINAL_SUMMARY:END -->
