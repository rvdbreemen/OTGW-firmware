---
id: TASK-289
title: Add -ffile-prefix-map to ESP32-S3 build for reproducibility
status: To Do
assignee: []
created_date: '2026-04-18 07:14'
labels:
  - esp32
  - build
  - reproducibility
dependencies:
  - TASK-287
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The deep-research flash-esp32 report recommends passing -ffile-prefix-map=$PWD=. to the compiler so that absolute source paths do not end up embedded in .o and .elf artifacts. Combined with the existing SOURCE_DATE_EPOCH/LC_ALL/TZ pinning from TASK-287, this makes ESP32-S3 builds byte-reproducible across machines. Low priority: only affects debug-section determinism; does not affect flash image behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 arduino-cli ESP32-S3 build passes --build-property compiler.cpp.extra_flags -ffile-prefix-map=<project>=. (and compiler.c/compiler.S variants)
- [ ] #2 PlatformIO ESP32 env passes the same flag via build_flags
- [ ] #3 Two consecutive builds on the same source tree produce identical app.bin sha256 when --reproducible is set
<!-- AC:END -->
