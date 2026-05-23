---
id: TASK-681
title: 'feat-2.0.0: port — FSexplorer readStringUntil → stack buffer (TASK-D)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:12'
updated_date: '2026-05-23 16:14'
labels: []
dependencies: []
ordinal: 57000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling port of dev TASK-D. Replace 'version = f.readStringUntil(chr10);' at FSexplorer.ino:359 with a char[32] stack buffer + f.readBytesUntil('\n', buf, sizeof(buf)-1) + null-terminate + trailing-CR trim. Mirrors helperStuff.ino:690 pattern. Eliminates one heap allocation per .hex file entry.

Master plan: /root/.claude/plans/evaluate-the-found-issues-misty-frog.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 f.readStringUntil('\n') replaced by char[32] + f.readBytesUntil + NUL terminator + trailing-CR trim
- [x] #2 version String local in the enclosing while-loop replaced by char[32] (or version String discarded if no longer needed)
- [x] #3 Behaviour unchanged for short version strings; long strings now truncated cleanly at 31 chars instead of growing the heap
- [x] #4 python build.py --firmware --target esp8266 exits 0
- [x] #5 python evaluate.py --quick: no new failures
- [ ] #6 Commit pushed to claude/beta20-improvements-2.0.0-xRpVI
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Match dev's broader scope (TASK-678 on dev, c72e4d33): replace String version/fwversion in apifirmwarefilelist() with char[32] stack buffers.
2. f.readStringUntil('\n') + version.trim() → readBytesUntil + NUL + trailing CR/whitespace trim.
3. fwversionBuf intermediate dropped; GetVersion writes directly into fwversion.
4. fwversion.length()/strcmp(...c_str()...) → fwversion[0] != '\0' && strcmp(fwversion, version) != 0.
5. version = fwversion → strlcpy.
6. f.print(version + '\n') → f.print(version); f.print('\n');
7. CSTR(version) call site: char[] is already a const char*, CSTR will accept it.
8. Build + evaluator.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-D-2.0.0 implementation complete.

- Replaced String version/fwversion with char[32] stack buffers (matching dev TASK-D scope c72e4d33 — broader than just line 359).

- f.readStringUntil + .trim() → f.readBytesUntil + NUL + trailing-CR/whitespace trim.

- fwversionBuf intermediate dropped; GetVersion writes directly into fwversion[32].

- fwversion.length() && strcmp(...c_str()...) → fwversion[0] != 0 && strcmp(fwversion, version) != 0.

- version = fwversion → strlcpy(version, fwversion, sizeof(version)).

- f.print(version + '\n') → f.print(version); f.print('\n').

- CSTR(version) call site: char[] satisfies CSTR's char* overload (OTGW-firmware.h:106).

- Build (ESP8266) clean, evaluator 60/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced String version/fwversion in apifirmwarefilelist() with char[32] stack buffers per ADR-004. Matches dev TASK-D scope (commit c72e4d33) which went beyond the master plan's literal scope (just the readStringUntil call) — both branches now eliminate the heap String allocation per-iteration AND the fwversionBuf intermediate.

Changes:
- char version[32], fwversion[32] replace String version, fwversion.
- f.readStringUntil + .trim() → f.readBytesUntil + NUL + trailing-CR/whitespace trim (pattern: helperStuff.ino:690).
- GetVersion(hexfile, fwversion, sizeof(fwversion)) writes directly into the new buffer; fwversionBuf intermediate removed.
- All downstream comparisons rewritten to char*: strcmp(fwversion, version), strlcpy, f.print(version) then f.print('\n').
- CSTR(version) at line 396 satisfies CSTR's char* overload.

Build (ESP8266): clean. Evaluator: 60/0 (identical to baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
