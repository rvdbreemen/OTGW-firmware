---
id: TASK-290
title: Add esptool image-info and verify-flash helpers to build.py
status: To Do
assignee: []
created_date: '2026-04-18 07:14'
labels:
  - esp32
  - build
  - release
dependencies:
  - TASK-287
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The deep-research flash-esp32 report recommends integrating esptool image-info, merge_bin (already done), verify-flash and read-flash into the build tooling so every release verifies flash layout integrity and factory images can be confidently shipped. build.py already emits merged images and a sha256 manifest via TASK-287 but does not validate them against the expected partition table on-chip.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.py --verify-image runs esptool image-info on each produced bin under build/ and parses the output to confirm flash_mode, flash_freq, flash_size and entry point match the target config in TARGETS[]
- [ ] #2 build.py --verify-flash PORT runs esptool verify-flash against a connected device for bootloader + partitions + app + filesystem at the documented offsets
- [ ] #3 Both commands exit non-zero on mismatch and log the delta
- [ ] #4 Documentation update in ch02 (hardware setup) EN+NL explaining when to use each verification helper
<!-- AC:END -->
