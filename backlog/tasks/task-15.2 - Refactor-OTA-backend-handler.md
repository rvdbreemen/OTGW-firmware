---
id: TASK-15.2
title: Refactor OTA backend handler
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-15 22:41'
updated_date: '2026-03-15 22:47'
labels:
  - refactor
  - ota
  - backend
dependencies: []
parent_task_id: TASK-15
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Refactor the OTA upload backend in OTGW-ModUpdateServer-impl.h into smaller helpers for start, write, end, and abort handling without changing OTA behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Upload handler responsibilities are split into smaller helpers
- [x] #2 LittleFS erase, watchdog feeding, settings restore, and reboot behavior remain unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Extract upload state reset into a helper.
2. Extract target-specific setup for firmware vs filesystem uploads.
3. Extract start, write, end, and abort handlers while preserving logging and flash semantics.
4. Re-run error checks on the updated file before moving on.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Split OTGW-ModUpdateServer upload handling into helper methods for start, write, end, abort, upload-size parsing, and target-specific setup while keeping erase, watchdog, settings restore, and reboot behavior intact.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refactored OTGW-ModUpdateServer upload handling into helpers for upload-size parsing, target setup, and start/write/end/abort stages while preserving watchdog feeding, full LittleFS erase, settings restore, telnet logging, and reboot behavior.
<!-- SECTION:FINAL_SUMMARY:END -->
