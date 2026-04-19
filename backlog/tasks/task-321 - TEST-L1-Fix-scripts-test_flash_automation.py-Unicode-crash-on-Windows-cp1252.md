---
id: TASK-321
title: '[TEST-L1] Fix scripts/test_flash_automation.py Unicode crash on Windows cp1252'
status: To Do
assignee: []
created_date: '2026-04-18 19:25'
labels:
  - testing
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
scripts/test_flash_automation.py prints check-mark characters directly; on the project's Windows dev environment this crashes with UnicodeEncodeError. Not wired into CI either.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Script uses the TextIOWrapper(sys.stdout.buffer, utf-8, replace) shim or plain ASCII [PASS]/[FAIL]
- [ ] #2 evaluate.py --quick invokes the script as part of the gate
- [ ] #3 Script runs green on Windows cp1252 and on Linux UTF-8
<!-- AC:END -->
