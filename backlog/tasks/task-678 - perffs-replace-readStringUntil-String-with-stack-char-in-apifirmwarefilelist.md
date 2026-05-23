---
id: TASK-678
title: >-
  perf(fs): replace readStringUntil/String with stack char[] in
  apifirmwarefilelist
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:06'
updated_date: '2026-05-23 16:08'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/FSexplorer.ino:321 uses f.readStringUntil('\n') (heap-allocating String) for short PIC version strings. Replace with a char[32] stack buffer + f.readBytesUntil + null-terminate, mirroring helperStuff.ino:738,790. Convert the related local 'version'/'fwversion' Strings to char[] so the surrounding code stays consistent (ADR-004 — no String in hot paths).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 version local converted to char[32], populated via readBytesUntil + NUL + CR trim
- [x] #2 fwversion converted to char[32] for consistency
- [x] #3 Downstream usages updated (compare, write-back, JSON emit)
- [x] #4 python build.py --firmware exits 0
- [x] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Declare char version[32]/fwversion[32]
2. readBytesUntil into version + NUL + CR trim
3. Update strcmp/length/copy/write-back/JSON emit to use C strings
4. Build firmware
5. Run evaluator quick
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced String version/fwversion locals in apifirmwarefilelist() with char[32] stack buffers. Reads the .ver file via f.readBytesUntil + NUL + trim (CR/space/tab), mirroring the pattern at helperStuff.ino:738,790. Dropped the redundant fwversionBuf intermediate by writing GetVersion directly into fwversion. Updated the four downstream usages: strcmp on char*, strlcpy for the write-back path, f.print + f.print('\\n') for the file write, and CSTR(char*) for the JSON emit (overload at OTGW-firmware.h:87). No heap allocations on this path now; behaviour unchanged for short version strings (longest seen is ~10 chars). Verified: python build.py --firmware exits 0; python evaluate.py --quick 34 passed, 0 failed (no delta vs baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
