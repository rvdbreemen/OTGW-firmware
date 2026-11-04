---
id: TASK-404
title: Remove BGTRACE probes and BGTASKS_TRACE macro from 2.0.0-beta branch
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 18:33'
updated_date: '2026-04-24 18:42'
labels:
  - cleanup
  - 2.0.0
  - merge-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the merged 2.0.0-beta branch the BGTRACE(...) probes that dev used in doBackgroundTasks() were dropped when 2.0.0 refactored the handler chain (handlePICSerial/loopOTDirect replaced handleOTGW/OTGWstream.loop). The macro infrastructure (BGTASKS_TRACE flag + BGTRACE(...) wrapper + the gated timing/logging call at OTGW-firmware.ino:142) is still present but unused. Remove the macro, the guard, and any remaining BGTASKS_TRACE-gated code so the source is clean. OTPROCESS_TRACE/OTTRACE is unrelated and stays untouched (handled by a separate task).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BGTRACE macro definition deleted from source
- [x] #2 BGTASKS_TRACE #define line removed
- [x] #3 Any #if BGTASKS_TRACE gated block including OTGW-firmware.ino:142 ESP.getMaxFreeBlockSize() call is removed
- [x] #4 pio run -e esp8266 and pio run -e esp32 still succeed
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate BGTRACE macro definition and BGTASKS_TRACE #define in OTGW-firmware.ino (around line 134-142 per H4 report).
2. Locate all remaining #if BGTASKS_TRACE gated blocks (expect: `_bgPrev`-using timing block around line 142 that calls ESP.getMaxFreeBlockSize()).
3. Search repo-wide for stray BGTRACE or BGTASKS_TRACE references (grep on .ino/.h/.cpp) — expect: 0 non-definition call sites per H1 report, but verify.
4. Delete: BGTASKS_TRACE #define, BGTRACE macro definition (and any helper like _bgPrev tracking), all #if BGTASKS_TRACE blocks, any documentation comments that reference BGTRACE.
5. pio run -e esp8266 (must succeed).
6. pio run -e esp32 (must succeed).
7. Commit with descriptive message.
8. Mark ACs done, add final summary, set status Done.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the unused BGTRACE macro infrastructure from OTGW-firmware.ino (27 lines deleted): BGTASKS_TRACE #define, BGTRACE(name) macro wrapper under #if BGTASKS_TRACE, and the no-op #else branch. The probes had already been dropped from the handler chain during the 2.0.0 refactor (handlePICSerial/loopOTDirect replaced handleOTGW/OTGWstream.loop); only the dead macro code remained.

**Changes:**
- `src/OTGW-firmware/OTGW-firmware.ino`: deleted lines 122-147 (comment block + #define BGTASKS_TRACE 0 + BGTRACE macro + #if/#else/#endif).

**Builds:**
- esp8266 (Core 2.7.4): SUCCESS in 51s
- esp32 (pioarduino 55.03.35): SUCCESS in 1m44s; RAM 32.0%, Flash 62.8% (unchanged vs baseline).

**Follow-up:**
- OTPROCESS_TRACE is unrelated and left untouched (handled by TASK-407).
- If BGTRACE-style instrumentation is needed again on 2.0.0, reintroduce it in the NEW handler chain (handlePICSerial/loopOTDirect) rather than cherry-picking the old version.

**Commit:** bb3f95f7
<!-- SECTION:FINAL_SUMMARY:END -->
