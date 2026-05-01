---
id: TASK-504
title: >-
  polish(otgw-core): fix "is in in queue" typo and clarify (%d) marker in
  checkCommandQueue log
status: In Progress
assignee: []
created_date: '2026-05-01 17:38'
updated_date: '2026-05-01 17:55'
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
- [ ] #4 python evaluate.py --quick passes
- [ ] #5 Build clean on ESP8266 and ESP32-S3 (python build.py)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Pure log-string polish in `OTGW-Core.ino`: (1) fix the duplicated "in in" at line 3029, (2) replace the trailing `(%d)` marker with `(len=%d)` at lines 3025 and 3033 so the printed number is self-describing as `strlen(buf)`. PROGMEM contract preserved (`F()` / `PSTR()` only). Validate with `python evaluate.py --quick`.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Three string edits in OTGW-Core.ino: line 3025 `(%d)` → `(len=%d)` (error variant), line 3029 typo `is in in queue` → `is in queue`, line 3033 `(%d)` → `(len=%d)` (success variant). PROGMEM contract preserved (F() / PSTR()). Trailing whitespace on lines 3025 and 3033 cleaned up as side effect. Diff: 3 insertions, 3 deletions.

AC 4 (`python evaluate.py --quick` passes): exit code 1, same pre-existing 14 PROGMEM-violations baseline as TASK-503. None of the 14 violations involve the touched lines. No regression. AC 4 strictly unticked pending baseline cleanup task. AC 5 (full ESP8266+ESP32-S3 build) deferred to parent (next consolidated build run).
<!-- SECTION:NOTES:END -->
