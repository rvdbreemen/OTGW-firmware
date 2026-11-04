---
id: TASK-1497
title: >-
  ota_flash.py: /version.hash does not prove a complete filesystem write; check
  a large file's length too
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T00:20:00Z'
updated_date: '2026-10-30T00:23:00Z'
labels:
  - tooling
  - ota
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 654000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1494 (coordinator 75, from d5's bench proof on .63, 2026-10-30). A dropped filesystem upload leaves a partly written partition: the early blocks, including /version.hash, are new; the later blocks of large files are not, and reads past that point return 0 (v2.js served cut at 65536 of 256745). TASK-1494 accepts the new /version.hash as proof of a complete write, which is not enough. Known limit of a length check: a device that sends a short Content-Length together with a short body is not caught; only an expected-size source (manifest or image parse) would catch that.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After the filesystem step ota_flash.py GETs /v2.js and fails before the app step when fewer bytes arrive than Content-Length (IncompleteRead) or the body is empty; covered by a host test with a fake server serving a short v2.js, red on a9324d302
- [x] #2 When device/info reports fsupdateincomplete:true (TASK-1495) ota_flash.py fails before the app step; an absent field counts as unknown, not as a failure; host test
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Stdlib only. After the /version.hash confirmation and the main-firmware wait, _check_fs_complete(): device/info fsupdateincomplete true -> SystemExit; GET /v2.js, http.client.IncompleteRead (fewer bytes than Content-Length) or an empty body -> SystemExit before the app step; a 503 from the file-serve gate is retried 3x. Docstring corrected: the v2.js cut on .63 came from the partial partition, not from the app/fs mix (d5's proof). Open-source check: trivial script logic, no library.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence (2026-10-30, wt-1494b on origin/dev a9324d302): tests/test_ota_flash.py on the fix: 9/9 OK. Against a9324d302's scripts/ota_flash.py (OTGW_OTA_FLASH_PY): FAILED failures=5, i.e. the 5 new tests (short v2.js 65536 of 256745, empty v2.js, device flag, busy 503 retried, whole+clear continues) are red and the 4 TASK-1494 tests stay green. evaluate.py --quick 79/0/0. Known limit (description): a short Content-Length with a matching short body is not caught. Not run against a real device (no bench flash in this lane). The TASK-1494 docstring claimed the 521-app/542-fs mix cut v2.js; d5 showed the partial partition did; corrected here.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ota_flash.py no longer treats a new /version.hash as proof of a complete filesystem write. Before the app step it stops when device/info reports fsupdateincomplete (TASK-1495 firmware) or when /v2.js arrives shorter than its Content-Length or empty; a 503 is retried. Evidence: tests/test_ota_flash.py 9/9 on the fix, 5 new tests red on a9324d302; evaluate --quick green. No device run.
<!-- SECTION:FINAL_SUMMARY:END -->
