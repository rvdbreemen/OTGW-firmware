---
id: TASK-1983
title: >-
  Firmware: report the running app's ELF sha256 in device/info and the core-dump
  response
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T20:07:00Z'
updated_date: '2026-11-16T07:27:00Z'
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

2026-11-16 08:07-08:18 bench (09, .61, bench build beta.342+cd59f4e): FINDING, the first shim was wrong. device/info appelfsha256 = '4cc905236' and X-Running-App-Sha256 = '4cc905236', 9 characters, a correct prefix of the .elf sha256 (4cc905236eeb...) but cut short. Cause, two sources: our sdkconfig CONFIG_APP_RETRIEVE_LEN_ELF_SHA=9 (framework-arduinoespressif32-libs/esp32s3/sdkconfig:506), and esp_app_get_elf_sha256() hands out at most that many characters; the full value is the 32 bytes of esp_app_desc_t.app_elf_sha256 (esp_app_desc.h:35). My earlier AC2 evidence was flawed: the host stub returned 64 characters, so it modelled what I expected, not the IDF. Fix: platformRunningAppSha() hex-encodes esp_app_get_description()->app_elf_sha256 (64 lowercase digits, once). The test stub now models the IDF limit (helper truncates to 9) and supplies the description bytes: red on the first shim ('fedcba987' != 64-char value), green on the fix (19/19). Coredump on .61 answered 200 (29,316 B) with X-Coredump-App-Sha256 3e1a8621a (an older image) next to the running one, so the header pair works. AC1 still waits for a device read of the 64-character value on the next build.
<!-- SECTION:NOTES:END -->
