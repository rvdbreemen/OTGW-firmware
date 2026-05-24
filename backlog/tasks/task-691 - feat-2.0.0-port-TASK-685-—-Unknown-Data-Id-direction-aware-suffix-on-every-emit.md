---
id: TASK-691
title: >-
  feat-2.0.0: port TASK-685 — Unknown-Data-Id direction-aware suffix on every
  emit
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-24 08:18'
updated_date: '2026-05-24 08:26'
labels:
  - port-from-dev
  - diagnostics
  - opentherm
dependencies: []
priority: medium
ordinal: 59000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-685 / commit de08130b. Adds direction-aware plain-English suffix on every slave Unknown-Data-Id frame: '(boiler does not implement)' for read direction, '(boiler rejected write)' for write direction. Introduces three function-static bitmaps in processOT(): lastMasterWasWrite, unknownLoggedRead, unknownLoggedWrite — populated unconditionally; ownership 'feeds the support map' (consumed in TASK-686 port). No log-line suppression.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Inside processOT(), three function-static uint8_t[32] bitmaps are added: lastMasterWasWrite, unknownLoggedRead, unknownLoggedWrite. Updated on every relevant frame: master Read-Data clears lastMasterWasWrite bit; master Write-Data sets it; slave OT_UNKNOWN_DATA_ID idempotently sets the matching unknownLogged{Read,Write} bit using lastMasterWasWrite as direction discriminator.
- [x] #2 The existing OT-bus log line gains a short direction-aware plain-English suffix on every Unknown-Data-Id slave frame: '(boiler does not implement)' for read direction, '(boiler rejected write)' for write direction. The suffix is appended via AddLog so the same string reaches both telnet (OTDebugT) and WebSocket OT Monitor (sendLogToWebSocket via ot_log_buffer).
- [x] #3 No log-line suppression introduced. Telnet emission for Unknown-Data-Id remains unconditional.
- [x] #4 Memory impact: 96 B static RAM in processOT (3 x 32-byte bitmaps).
- [x] #5 python build.py --firmware exits 0.
- [x] #6 python evaluate.py --quick shows no new failures vs current 2.0.0 baseline.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add three function-static uint8_t[32] bitmaps in processOT() (lastMasterWasWrite, unknownLoggedRead, unknownLoggedWrite) at the same location as the existing static fields (epochBoilerlastseen et al).
2. After the OTmap lookup block (at the empty-line spot post-line 4123), insert a small classification block: master frame OT_READ_DATA clears the lastMasterWasWrite bit; OT_WRITE_DATA sets it; slave OT_UNKNOWN_DATA_ID idempotently sets the matching unknownLogged{Read,Write} bit.
3. Before AddLogln() at the existing line 4196, inject the AddLog suffix branch for slave Unknown-Data-Id: append '(boiler does not implement)' or '(boiler rejected write)' depending on lastMasterWasWrite.
4. Bump prerelease (alpha.58 -> alpha.59).
5. Build + evaluator.
6. Commit.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev TASK-685 to 2.0.0: revised slave Unknown-Data-Id logging to emit a plain-English direction-aware suffix on every occurrence (not just the first), so a later-connecting telnet tester still sees the diagnostic context.

## Changes
- processOT(): added three 32-byte function-static bitmaps (lastMasterWasWrite, unknownLoggedRead, unknownLoggedWrite) immediately after the existing per-call statics.
- Inserted a small classification block right after the OTmap lookup: master Read-Data clears the lastMasterWasWrite bit; master Write-Data sets it; slave OT_UNKNOWN_DATA_ID idempotently sets the matching unknownLogged{Read,Write} bit using lastMasterWasWrite as direction discriminator. The bitmaps feed the support-map surfaces in TASK-692/693 ports.
- Inserted an AddLog suffix branch right before AddLogln(): when masterslave==1 and type==OT_UNKNOWN_DATA_ID, append '(boiler does not implement)' for read direction or '(boiler rejected write)' for write direction. The suffix reaches both telnet (OTDebugT) and the WebSocket OT Monitor (sendLogToWebSocket) via the shared ot_log_buffer.
- No log-line suppression introduced. Telnet emission for Unknown-Data-Id remains unconditional.

## Notes vs dev
- 2.0.0's processOT() carries an extra parameter (suppressOutput, TASK-293) for OT-direct PS=1; the bitmap declarations live alongside the existing per-call statics inside processOT and run unconditionally — the suffix branch sits inside the same masterslave==1/type==OT_UNKNOWN_DATA_ID guard as dev, no behaviour difference.
- 2.0.0 uses OTDebugT (macro defined in OTGW-Core.ino), not the dev-side OTGWDebugT. The new AddLog suffix path does not call any debug helper directly — it just composes into ot_log_buffer that the existing OTDebugT/sendLogToWebSocket calls already consume.

## Memory
96 B static RAM in processOT (3 × 32-byte bitmaps). No heap allocations.

## Verification
- python build.py --firmware --target esp8266 exits 0 (2.0.0-alpha.59).
- python evaluate.py --quick: 60/1/0 (98.5%, unchanged from baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
