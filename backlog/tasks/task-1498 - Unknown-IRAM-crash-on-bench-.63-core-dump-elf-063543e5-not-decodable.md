---
id: TASK-1498
title: 'Unknown IRAM crash on bench .63: core dump elf 063543e5, not decodable'
status: To Do
assignee: []
created_date: '2026-10-30T04:19:00Z'
labels:
  - bug
  - esp32
  - crash
dependencies: []
priority: low
ordinal: 654000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-30 05:10:58 while bench-testing TASK-1479 on .63 (Classic + S3, AC:27:6E:CE:45:D8). The first boot of the TASK-1479 test build reported a core dump that was already in .63's coredump partition, from an earlier crash on another build:

Core dump: loopTask pc=0x403828c1 elf=063543e5 bt=0x403828c1 0x40382889 0x40389ae2 0x4038329a 0x403833c2 0x40377949

The task was loopTask, and the PC and every backtrace address are in IRAM (0x403xxxxx/0x4037xxxx). That points at an ISR, a flash-cache-disabled path or a FreeRTOS/IDF IRAM routine, not at normal application code.

The dump cannot be decoded: no ELF with sha256 prefix 063543e5 was found. Searched:
- every */build/*.elf under RvdB;
- %LOCALAPPDATA%/OTGW-capture (63 elfs);
- OTGW-build-archive, every 2.0.0-alpha >= 470 zip (92 combo elfs);
- 521+2425108, the build .63 ran from 22:56 (that is 2c30802c);
- 542+c00c5f0, which is 6bb374f7.
The dump can be older than 22:56, because it stays in flash until a new crash overwrites it.

The dump itself is gone from the partition list only when overwritten; .63 later took a TWDT dump from the TASK-1479 Z test (elf 90ec453b), so the raw flash image no longer holds it. This line is the only record.

Low priority: there is one occurrence and no build to match it. Reopen if a later TASK-1479-enabled build reports an IRAM loopTask dump with a known ELF.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 If a matching ELF turns up (an older build-archive zip or another session's build), decode the backtrace with xtensa-esp32s3-elf-addr2line and record the frames
- [ ] #2 If a new IRAM loopTask dump appears on a build with TASK-1479, link it here and decode it
<!-- AC:END -->
