---
id: TASK-504
title: >-
  polish(otgw-core): fix "is in in queue" typo and clarify (%d) marker in
  checkCommandQueue log
status: Done
assignee: []
created_date: '2026-05-01 17:38'
updated_date: '2026-05-01 18:01'
labels:
  - polish
  - log-quality
  - follow-up
dependencies: []
references:
  - src/OTGW-firmware/OTGW-Core.ino
  - Found while analyzing Georges SAT telnet log 2026-05-01
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Problem

Two cosmetic log-text issues in `OTGW-Core.ino` around the command-queue response handler (HEAD line 3029-3033):

1. **Typo**: `OTDebugT(F("CmdQueue: Checking if command is in in queue ["))` — duplicated `in`. Should read `Checking if command is in queue`. Confirmed observed in production telnet logs (Georges build 3e14c2c, log captured 2026-05-01).

2. **Ambiguous trailing marker**: the closing `OTDebugf(PSTR("] (%d)\r\n"), len)` produces output like `[CS: 62.00] (9)`. The `(N)` is `strlen(buf)` (response-buffer length), but two readers today — myself and likely future operators — first interpreted it as queue depth or queue index. Renaming the format to `(len=%d)` makes it self-describing.

## Where

`src/OTGW-firmware/OTGW-Core.ino:3029` and `:3033` (the matching close-bracket print). Same pattern likely exists at lines 3021/3025 for the `Error: Not a command response` variant — apply consistently.

## Why low priority

Pure log-string change, zero behavioural impact. Useful when grepping production logs and when reading the source — saves the next reader a moment of confusion. Bundle with the next polish/Lows batch (post-TASK-502 follow-up batch F if one materializes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No occurrences of 'is in in queue' remain anywhere in src/OTGW-firmware/
- [x] #2 The closing log marker for both 'Checking if command is in queue' and 'Error: Not a command response' variants reads (len=%d) instead of (%d)
- [x] #3 All affected strings remain in flash via F()/PSTR() macros (PROGMEM contract per ADR-004)
- [x] #4 python evaluate.py --quick passes
- [x] #5 Build clean on ESP8266 and ESP32-S3 (python build.py)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Pure log-string polish in `OTGW-Core.ino`: (1) fix the duplicated "in in" at line 3029, (2) replace the trailing `(%d)` marker with `(len=%d)` at lines 3025 and 3033 so the printed number is self-describing as `strlen(buf)`. PROGMEM contract preserved (`F()` / `PSTR()` only). Validate with `python evaluate.py --quick`.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Three string edits in OTGW-Core.ino: line 3025 `(%d)` → `(len=%d)` (error variant), line 3029 typo `is in in queue` → `is in queue`, line 3033 `(%d)` → `(len=%d)` (success variant). PROGMEM contract preserved (F() / PSTR()). Trailing whitespace on lines 3025 and 3033 cleaned up as side effect. Diff: 3 insertions, 3 deletions.

AC 4 (`python evaluate.py --quick` passes): exit code 1, same pre-existing 14 PROGMEM-violations baseline as TASK-503. None of the 14 violations involve the touched lines. No regression. AC 4 strictly unticked pending baseline cleanup task. AC 5 (full ESP8266+ESP32-S3 build) deferred to parent (next consolidated build run).

Correction on AC 4: rechecked `python evaluate.py --quick` post-commit, exit code is **0**, not 1 as I and the implementer agent both initially read. The 14 PROGMEM-violations finding is non-fatal in the script: overall exit 0, Health Score 95.5%. AC 4 (`python evaluate.py --quick` passes) is therefore met as written. AC 5 (full ESP8266+ESP32-S3 build) pending background build run; will tick on green build.

AC 5 validated: `python build.py` exit 0. Both ESP8266 and ESP32 build artifacts produced cleanly at hash 589968d (matches HEAD). Distribution zips and merged firmware images generated. Build validates this commit alongside the rest of today's batch.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented in commit 589968d0. Three string edits in `OTGW-Core.ino:checkCommandResponse` around lines 3022-3033: typo `is in in queue` -> `is in queue`, and `(%d)` -> `(len=%d)` on both the error and success log variants so the printed number is self-describing as `strlen(buf)`. PROGMEM contract preserved (F() / PSTR() unchanged). Trailing whitespace cleaned up as side effect of the exact-match edits. All 5 ACs met. Build clean on both ESP8266 and ESP32 verified at HEAD 589968d.
<!-- SECTION:FINAL_SUMMARY:END -->
