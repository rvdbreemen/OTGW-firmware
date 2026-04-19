---
id: TASK-308
title: >-
  [TEST-M1] Add smoke tests for build.py
  --verify-image/--verify-flash/--reproducible flags
status: To Do
assignee: []
created_date: '2026-04-18 19:21'
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
- [ ] #1 tests/test_build.py runs python build.py --help and asserts each new flag is listed
- [ ] #2 Captured esptool image-info fixture feeds a _parse_image_header helper, asserting mode/freq/size
- [ ] #3 evaluate.py --quick invokes test_build.py
<!-- AC:END -->
