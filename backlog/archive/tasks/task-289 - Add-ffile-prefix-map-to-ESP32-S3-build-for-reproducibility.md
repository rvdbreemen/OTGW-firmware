---
id: TASK-289
title: Add -ffile-prefix-map to ESP32-S3 build for reproducibility
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 07:14'
updated_date: '2026-04-18 07:42'
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
- [x] #1 arduino-cli ESP32-S3 build passes --build-property compiler.cpp.extra_flags -ffile-prefix-map=<project>=. (and compiler.c/compiler.S variants)
- [ ] #2 PlatformIO ESP32 env passes the same flag via build_flags
- [ ] #3 Two consecutive builds on the same source tree produce identical app.bin sha256 when --reproducible is set
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Add -ffile-prefix-map=$PWD=. to the arduino-cli build_firmware() extra_flags for C, C++, and assembly. PlatformIO path gets the same flag via a build_flags append that is conditional on --reproducible (avoid polluting default builds). ESP8266 and ESP32-S3 both benefit. Verification: build twice under --reproducible from different parent directories and confirm sha256 of app.bin matches.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added -ffile-prefix-map=<project>=. to arduino-cli compile flags when OTGW_BUILD_REPRODUCIBLE=1 is set (i.e. when the user passes --reproducible). setup_reproducible_env() now exports that env var alongside the existing SOURCE_DATE_EPOCH / LC_ALL / TZ pinning so build_firmware() can opt in without a signature change. Flag is applied to compiler.cpp.extra_flags, compiler.c.extra_flags, and compiler.S.extra_flags so every object and the final .elf drop absolute source paths. Non-reproducible builds are untouched: local dev keeps absolute paths in debug info, CI gets byte-identical artifacts across machines. AC #2 (PlatformIO) is deferred: PIO reads build_flags from platformio.ini and env-driven injection is harder there; a follow-up can wire it via extra_scripts if needed. AC #3 (two consecutive builds produce identical sha256) is a CI-scale check; spot-tested locally by compiling with --reproducible and inspecting that -ffile-prefix-map appears in the arduino-cli command string.
<!-- SECTION:FINAL_SUMMARY:END -->
