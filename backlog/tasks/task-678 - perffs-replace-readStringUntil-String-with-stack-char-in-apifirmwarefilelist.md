---
id: TASK-678
title: >-
  perf(fs): replace readStringUntil/String with stack char[] in
  apifirmwarefilelist
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-23 16:06'
updated_date: '2026-05-23 16:06'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/FSexplorer.ino:321 uses f.readStringUntil('\n') (heap-allocating String) for short PIC version strings. Replace with a char[32] stack buffer + f.readBytesUntil + null-terminate, mirroring helperStuff.ino:738,790. Convert the related local 'version'/'fwversion' Strings to char[] so the surrounding code stays consistent (ADR-004 — no String in hot paths).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 version local converted to char[32], populated via readBytesUntil + NUL + CR trim
- [ ] #2 fwversion converted to char[32] for consistency
- [ ] #3 Downstream usages updated (compare, write-back, JSON emit)
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Declare char version[32]/fwversion[32]
2. readBytesUntil into version + NUL + CR trim
3. Update strcmp/length/copy/write-back/JSON emit to use C strings
4. Build firmware
5. Run evaluator quick
<!-- SECTION:PLAN:END -->
