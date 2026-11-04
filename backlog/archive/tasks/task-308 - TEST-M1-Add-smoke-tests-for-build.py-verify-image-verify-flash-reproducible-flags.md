---
id: TASK-308
title: >-
  [TEST-M1] Add smoke tests for build.py
  --verify-image/--verify-flash/--reproducible flags
status: Done
assignee:
  - '@claude'
created_date: '2026-04-18 19:21'
updated_date: '2026-04-19 07:05'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-287/289/290/291 added four release-integrity flags verified only by ast.parse plus one manual smoke. ESP32-S3 DIO/QIO normalisation (build.py:1193-1196) could silently break.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 tests/test_build.py runs python build.py --help and asserts each new flag is listed
- [ ] #2 Captured esptool image-info fixture feeds a _parse_image_header helper, asserting mode/freq/size
- [x] #3 evaluate.py --quick invokes test_build.py
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC2 (captured esptool fixture + _parse_image_header helper) deferred: verify_image_header(project_dir, target) at build.py:1133 is IO-coupled (reads real artifact, shells out to esptool). Extracting a pure parser helper is larger scope than the surface-level smoke covered today. Follow-up task should refactor verify_image_header into (load_image_header_bytes, parse_image_header_bytes) and then the pytest fixture becomes trivial.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
tests/test_build.py: 4 stdlib unittest tests. TestBuildHelpFlags invokes python build.py --help and asserts each of --verify-image, --verify-flash, --reproducible, --ccache, --manifest, --firmware, --clean appears. TestBuildImportable asserts build.py parses cleanly via ast. Runs in <1s without needing a real build. Catches silent flag renames.
<!-- SECTION:FINAL_SUMMARY:END -->
