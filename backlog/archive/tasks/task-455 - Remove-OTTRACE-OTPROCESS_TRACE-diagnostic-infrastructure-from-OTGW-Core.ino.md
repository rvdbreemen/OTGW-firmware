---
id: TASK-455
title: Remove OTTRACE / OTPROCESS_TRACE diagnostic infrastructure from OTGW-Core.ino
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 19:53'
updated_date: '2026-04-27 20:03'
labels:
  - cleanup
  - tracing
  - 2.0.0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Context

`OTTRACE(...)` / `OTPROCESS_TRACE` were introduced via TASK-397 to diagnose heap behaviour inside `processOT()` (decode → publish → WebSocket send). That diagnostic mission is complete: the underlying issues have been resolved in subsequent tasks, and the macro is permanently disabled (`OTPROCESS_TRACE 0`) — currently dead infrastructure paying rent only in cognitive load.

The companion **BGTRACE** macro family was already removed in commit `bb3f95f7` via TASK-404. Verified: `git grep` for `BGTRACE|BGTASKS_TRACE` returns zero hits in `src/`.

## Scope

All in `src/OTGW-firmware/OTGW-Core.ino`:

1. **Definition block (~lines 3861-3886, 26 lines)**: comment header explaining TASK-397 purpose + `#define OTPROCESS_TRACE 0` + `#if OTPROCESS_TRACE / #define OTTRACE(...) do {...} while(0) / #else / #define OTTRACE(name) ((void)0) / #endif`.
2. **Local-declaration block in `processOT()` (~lines 4074-4081, 7 lines)**: 3-line comment, `#if OTPROCESS_TRACE` guard around `_otBaselineHeap` + `_otPrev` declarations, `#endif`.
3. **Four `OTTRACE(...)` call-sites in `processOT()`**:
   - `OTTRACE("pre-decode");` (~line 4081)
   - `OTTRACE("post-decode");` (~line 4091)
   - `OTTRACE("post-debug");` (~line 4096)
   - `OTTRACE("post-ws");` (~line 4100)

## Out of scope

- BGTRACE: already removed (TASK-404).
- Documentation references in `docs/`, `backlog/tasks/task-397/398/403/404/407/...`, ADR-082 — historical context, leave intact unless cleaning up retired-task docs is a separate concern.
- TASK-451 implementation notes mention the OTPROCESS_TRACE wrap as a fix; do NOT retroactively edit (TASK-451 is Done; reading git history will explain).

## Risk

- Low. The macro is currently a no-op (`((void)0)`); removing the call-sites changes nothing at runtime. Build must remain clean on both ESP8266 and ESP32.
- One subtle gotcha: ensure no comment elsewhere in `processOT()` references variable names `_otPrev` / `_otBaselineHeap` after their declarations are gone.

## Verification

- ESP8266 + ESP32 build clean (exit 0, no new warnings, expect `-N` warning-count drop where N is the count of "unused variable" warnings the trace block was producing if any — likely 0 since the wrap I just landed in TASK-451 already silenced them).
- `git grep` for `OTTRACE|OTPROCESS_TRACE` in `src/` returns zero hits.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTPROCESS_TRACE definition + OTTRACE macro definitions deleted from OTGW-Core.ino
- [x] #2 Local-declaration block + #if guard at processOT entry deleted
- [x] #3 All four OTTRACE() call-sites in processOT removed
- [x] #4 git grep -E 'OTTRACE|OTPROCESS_TRACE' src/ returns zero hits
- [x] #5 ESP8266 incremental build exits 0 with no new warnings
- [x] #6 ESP32 incremental build exits 0 with no new warnings
- [x] #7 Functional smoke test: processOT still works (no syntax / linker error from leftover references)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed (2026-04-27 19:55):
- Definition block (~26 lines: TASK-397 comment header + #define OTPROCESS_TRACE 0 + #if/#endif around OTTRACE macro and the ((void)0) #else branch) deleted from OTGW-Core.ino around lines 3861-3886 (in HEAD numbering).
- Local-decl block + #if guard (7 lines) at processOT() entry deleted, including the 3-line comment about OTTRACE consumption.
- All four OTTRACE() call-sites removed: 'pre-decode', 'post-decode', 'post-debug', 'post-ws'.
- Combined into one Edit covering the whole processOT() body region for cleaner atomic change.
- grep -E 'OTTRACE|OTPROCESS_TRACE|_otBaselineHeap|_otPrev' src/ returns ZERO hits.
- Build exit 0 for both ESP8266 and ESP32. Warning count went from 61 (TASK-451 baseline) to 62; the +1 is `otDirectBridgeProcessPRResponse` defined in OTDirect.ino:1778 (a separate agent's improvement, unrelated to this task — confirmed by user).
- BGTRACE confirmed already removed in commit bb3f95f7 via TASK-404 (zero hits in src/ for BGTRACE|BGTASKS_TRACE).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
## Summary

Removed the OTTRACE / OTPROCESS_TRACE diagnostic infrastructure from `OTGW-Core.ino`. The macro family was introduced via TASK-397 to diagnose heap behaviour inside `processOT()` (decode → publish → WebSocket send); that mission is complete and the macro has been permanently disabled (`OTPROCESS_TRACE 0`) — currently dead infrastructure paying rent only in cognitive load.

## What was removed

- Definition block at `OTGW-Core.ino:3861-3886` (26 lines): TASK-397 comment header, `#define OTPROCESS_TRACE 0`, `#if OTPROCESS_TRACE / #define OTTRACE(name) do {...} while(0) / #else / #define OTTRACE(name) ((void)0) / #endif`.
- Local-declaration block at `processOT()` entry (7 lines): 3-line comment about OTTRACE consumption, `#if OTPROCESS_TRACE` guard around `_otBaselineHeap` + `_otPrev` declarations, `#endif`.
- All four `OTTRACE(...)` call-sites in `processOT()`: pre-decode, post-decode, post-debug, post-ws.

## What was NOT removed

- BGTRACE / BGTASKS_TRACE: already removed in commit `bb3f95f7` via TASK-404. Verified zero hits.
- Documentation references in `docs/`, `backlog/tasks/task-397/398/403/404/407/...`, ADR-082: historical context, intentionally left intact.
- TASK-451 implementation notes mentioning the OTPROCESS_TRACE wrap as a fix: not retroactively edited (TASK-451 is Done; git history will explain).

## Verification

- `git grep -E 'OTTRACE|OTPROCESS_TRACE|_otBaselineHeap|_otPrev' src/` returns zero hits.
- ESP8266 + ESP32 build clean (exit 0).
- Warning count change relative to TASK-451 baseline: +1 (62 vs 61), but the +1 is `otDirectBridgeProcessPRResponse(const char*)` — a recent OTDirect addition by a separate agent's work, unrelated to this task. Confirmed by user.
- No new warnings caused by this task.

## Files touched

- `src/OTGW-firmware/OTGW-Core.ino` (one file, two non-overlapping deletions: the definition block, and the consolidated processOT body region containing the local-decl block + four call-sites).
<!-- SECTION:FINAL_SUMMARY:END -->
