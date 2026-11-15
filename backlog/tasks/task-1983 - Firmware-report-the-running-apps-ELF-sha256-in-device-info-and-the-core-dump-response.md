---
id: TASK-1983
title: >-
  Firmware: report the running app's ELF sha256 in device/info and the core-dump
  response
status: To Do
assignee: []
created_date: '2026-11-15T20:07:00Z'
updated_date: '2026-11-15T20:10:00Z'
labels:
  - capture
  - coredump
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1013000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1972 AC3 (coordinator decision 2026-11-15: not now, own low-priority task). The core-dump partition is never erased, and the reboot log that names a crash keeps 20 lines, so after enough reboots a served dump has no log line left to date it. The dump carries its app ELF sha256 (X-Coredump-App-Sha256, restAPI.ino sendDeviceCoreDump()), but the firmware does not publish the running image's ELF sha256 (version.hash is the githash, a different value). With that field otgw_capture.py coredump_age() can say 'dump of this build' or 'dump of an earlier build' exactly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 device/info carries the running image's ELF sha256 (esp_app_get_elf_sha256) and GET /api/v2/device/coredump adds it as a header next to X-Coredump-App-Sha256; openapi updated
- [ ] #2 Host test on the field and the header, red on the old code and green on the fix
- [ ] #3 otgw_capture.py coredump_age() uses it (same build vs an earlier build) with a test
<!-- AC:END -->
