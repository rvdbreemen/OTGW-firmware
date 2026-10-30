---
id: TASK-1494
title: >-
  ota_flash.py: a dropped response after the filesystem write aborts the run and
  leaves app/filesystem mismatched
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T23:52:00Z'
updated_date: '2026-10-30T00:04:00Z'
labels:
  - tooling
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 652000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 on 2026-10-30 (bench .63, alpha.521 to alpha.542). POST /update?cmd=100&size=786432 wrote the filesystem (GET /version.hash returned the new c00c5f0), but the device closed the connection without a response, probably during the remount. The script raised http.client.RemoteDisconnected and aborted before the app step (safeboot), leaving app 521 + fs 542. On that mix the 521 firmware served the larger minified v2.js cut at 65536 of 256745 bytes, so the v2 UI cannot start. Evidence: d5 scratchpad bench542/ (ota_flash.log, settings and device_info before/after, served_* files).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a dropped or missing response to the filesystem POST, ota_flash.py polls /version.hash (with a timeout) and continues only when it shows the new hash; otherwise it reports a clear failure; covered by a host test with a fake server that drops the response (red on HEAD)
- [x] #2 The app step order and the fs/app mismatch risk are documented in the script help or docs
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Keep the stdlib-only script. flash_fs(): read /version.hash before the write; the expected hash is the +<githash> in the .littlefs.bin name (fallback: any hash other than the old one). A dropped/missing response (http.client.HTTPException or OSError, but not HTTPError) is logged, then /version.hash is polled for FS_VERIFY_TIMEOUT (120 s); continue to the app step only on the expected hash, else SystemExit before the app step. The normal-response path checks the hash too. Docstring/--help documents the order and the fs/app mismatch risk. Host test with a fake http.server that drops the POST response. Open-source check: trivial script logic, no library.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (2026-10-30, wt-1494 on origin/dev c00c5f025): tests/test_ota_flash.py on the fix: 4/4 OK (exit 0). The same test against HEAD's scripts/ota_flash.py (OTGW_OTA_FLASH_PY): FAILED failures=2 errors=2 (exit 1); both drop cases raise http.client.RemoteDisconnected, the exception from .63. evaluate.py --quick: 79 passed, 0 warn, 0 fail. AC#2: the module docstring is the --help text (RawDescriptionHelpFormatter), checked with --help. NOT verified on hardware: coordinator 75 said not to flash a bench with it (d5 is on .63). Tooling only, no firmware bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ota_flash.py no longer aborts when the device drops the connection after the filesystem write. It polls /version.hash until the image's githash appears and only then does the app step; otherwise it stops with a clear error before the app step. Evidence: tests/test_ota_flash.py 4/4 on the fix, red on HEAD (RemoteDisconnected as on .63); evaluate --quick green. No bench run (by instruction).
<!-- SECTION:FINAL_SUMMARY:END -->
