---
id: TASK-15.3
title: Clarify flash-mode runtime handling
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-15 22:41'
updated_date: '2026-03-15 22:47'
labels:
  - refactor
  - ota
  - runtime
dependencies: []
parent_task_id: TASK-15
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Clarify flash-mode runtime gating in OTGW-firmware.ino by extracting named helpers for ESP and PIC flash task handling without changing semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Flash-mode task handling is extracted into named helpers
- [x] #2 loopWifi gating and ESP/PIC flash behavior remain unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Extract ESP flash background-task handling into a named helper.
2. Extract PIC flash background-task handling into a named helper.
3. Replace the inline branches in doBackgroundTasks() with the helpers.
4. Preserve loopWifi gating and the exact services enabled in each flash mode.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extracted named helpers for ESP flash and PIC flash background-task handling in OTGW-firmware.ino and replaced the inline branches in doBackgroundTasks() without changing loopWifi gating or enabled services.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Clarified flash-mode runtime handling by extracting dedicated helpers for ESP flash and PIC flash background tasks in OTGW-firmware.ino while preserving loopWifi gating and the services enabled in each flash mode.
<!-- SECTION:FINAL_SUMMARY:END -->
