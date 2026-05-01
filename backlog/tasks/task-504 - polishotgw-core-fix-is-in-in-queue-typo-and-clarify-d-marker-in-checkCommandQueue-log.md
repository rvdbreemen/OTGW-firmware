---
id: TASK-504
title: >-
  polish(otgw-core): fix "is in in queue" typo and clarify (%d) marker in
  checkCommandQueue log
status: To Do
assignee: []
created_date: '2026-05-01 17:38'
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
- [ ] #1 No occurrences of 'is in in queue' remain anywhere in src/OTGW-firmware/
- [ ] #2 The closing log marker for both 'Checking if command is in queue' and 'Error: Not a command response' variants reads (len=%d) instead of (%d)
- [ ] #3 All affected strings remain in flash via F()/PSTR() macros (PROGMEM contract per ADR-004)
- [ ] #4 python evaluate.py --quick passes
- [ ] #5 Build clean on ESP8266 and ESP32-S3 (python build.py)
<!-- AC:END -->
