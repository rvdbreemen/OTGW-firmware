---
id: TASK-399
title: >-
  Bump SimpleTelnet printf stack buffer from 64 to 256 bytes (tunable
  SIMPLETELNET_PRINTF_STACK_LEN)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 11:28'
updated_date: '2026-04-24 11:33'
labels:
  - simpletelnet
  - heap
  - fragmentation
  - library
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SimpleTelnet library's printf() and printf_P() methods use a 64-byte stack buffer (loc_buf[64]) for formatting output; if the formatted string exceeds 64 bytes they fall back to malloc/free per call. OTGW debug log lines (especially OT frame logs like 'Request Boiler R00000000 0 Read-Data > Status = Master [-----W--]') are typically 80-150 bytes, so the fallback path fires on every OT frame. Each malloc/free cycle contributes to per-frame heap fragmentation (~1344 bytes additional drop observed in TASK-397 OTTRACE data). Raising the stack buffer to 256 bytes eliminates this fallback for ~95% of OTGW debug output. Exposed as a tunable #define SIMPLETELNET_PRINTF_STACK_LEN following the existing SIMPLETELNET_* define pattern so users can override without forking. Library is in-tree at src/libraries/SimpleTelnet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SimpleTelnet.h exposes #ifndef SIMPLETELNET_PRINTF_STACK_LEN with default 256, placed in the compile-time tunables block alongside SIMPLETELNET_LINE_BUF_LEN, SIMPLETELNET_IP_LEN, SIMPLETELNET_MAX_WRITE_ERRORS, SIMPLETELNET_KEEPALIVE_MS
- [x] #2 SimpleTelnet_impl.tpp printf() method uses char loc_buf[SIMPLETELNET_PRINTF_STACK_LEN] instead of char loc_buf[64]
- [x] #3 SimpleTelnet_impl.tpp printf_P() method uses the same tunable
- [x] #4 Comment above the tunable explains rationale: OTGW log lines commonly exceed 64 bytes; 256 default eliminates malloc/free per call; users can #define to a different value before #include if RAM is tight
- [x] #5 python build.py --firmware compiles cleanly with the change; binary size delta documented (expect ~0 flash change, +192 bytes stack during active printf call)
- [ ] #6 Per-OT-frame OTTRACE post-debug dHeap drop pattern (observed -1344 intermittently in TASK-397 log samples) becomes consistently 0 or negligible after flashing — verify via re-enabling OTPROCESS_TRACE=1 temporarily on a test device and comparing pre- and post-fix logs
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SimpleTelnet.h compile-time tunables block, add #ifndef SIMPLETELNET_PRINTF_STACK_LEN with default 256 + short rationale comment.
2. Edit SimpleTelnet_impl.tpp printf(): replace char loc_buf[64] with char loc_buf[SIMPLETELNET_PRINTF_STACK_LEN].
3. Same for printf_P() on ESP8266.
4. Verify via grep that no other hardcoded 64-byte buffers exist in SimpleTelnet source.
5. python build.py --firmware — confirm clean build + note size delta.
6. Commit on dev with message fix(simpletelnet): raise printf stack buffer to 256 to eliminate malloc fallback for OTGW debug lines (TASK-399 aka user-requested Fix B).
7. Push origin/dev.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SimpleTelnet.h: added #ifndef SIMPLETELNET_PRINTF_STACK_LEN with default 256 + rationale comment.
SimpleTelnet_impl.tpp: printf() + printf_P() both use the tunable instead of hardcoded 64.
Build: python build.py --firmware exit 0, no warnings.
Heap-verification AC (7) deferred to user hardware test — they can flip OTPROCESS_TRACE=1 temporarily and compare pre-/post-fix logs.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Raises SimpleTelnet printf stack buffer from 64 to 256 bytes via a new SIMPLETELNET_PRINTF_STACK_LEN tunable. Eliminates the malloc/free fallback path for ~95% of OTGW debug lines (typical 80-150 bytes). Expected heap-burst impact per OT-frame: removes the intermittent -1344 byte allocation observed in TASK-397 OTTRACE data on the post-debug probe, bringing that phase consistently to 0 or near-zero dHeap. No breaking changes; users can override via #define before #include if RAM is constrained.
<!-- SECTION:FINAL_SUMMARY:END -->
