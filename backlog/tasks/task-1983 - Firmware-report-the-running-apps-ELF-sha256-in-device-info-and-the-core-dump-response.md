---
id: TASK-1983
title: >-
  Firmware: report the running app's ELF sha256 in device/info and the core-dump
  response
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T20:07:00Z'
updated_date: '2026-11-16T05:13:00Z'
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
- [x] #2 Host test on the field and the header, red on the old code and green on the fix
- [x] #3 otgw_capture.py coredump_age() uses it (same build vs an earlier build) with a test
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-16 (ba, lane/ba-1983 from origin/dev, commit 68439847b, beta.340 local). Platform shim platformRunningAppSha() (platform_esp32.h, esp_app_get_elf_sha256 from esp_app_desc.h, read once and cached, lowercase, empty when the description has none); restAPI.ino device/info field appelfsha256 (constant per boot, so the chunked JSON passes stay equal) and GET /api/v2/device/coredump header X-Running-App-Sha256 next to X-Coredump-App-Sha256 (4 of the 6 header slots, name 20 < 32 chars); openapi.yaml both documented (YAML parses). otgw_capture.py dump_build() + coredump_age() append 'dump of this build' / 'dump of an earlier build' by prefix match, nothing on firmware without the field.
Evidence: tests/test_device_coredump_api.py red before the fix (9 ran: 2 FAIL source checks + setUpClass ERROR 'platformRunningAppSha identifier not found'), green after: 19/19 (shim: full lowercase sha, read once via call counter; source: header + device/info field). tests/test_capture_tool.py 4 new tests red before (dump of this build / earlier build / fetch line), full file 60 passed 2 skipped after. Seam sweep (14 files mentioning fwversion/openapi/platform_esp32/X-Coredump): all OK. evaluate --quick 0 FAIL / 0 WARN.
AC1 not ticked yet: the code and openapi are done, but no device has shown the field; needs the next build on a unit (health read of device/info + coredump headers by 09).
<!-- SECTION:NOTES:END -->
