---
id: TASK-15.1
title: Refactor OTA page JavaScript
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-15 22:41'
updated_date: '2026-03-15 22:44'
labels:
  - refactor
  - ota
  - webui
dependencies: []
parent_task_id: TASK-15
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Refactor the update page JavaScript in updateServerHtml.h to eliminate duplicated reboot polling and label-restore flow while preserving browser behavior.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Shared reboot/health-check helper is used by both OTA entry points
- [x] #2 No change to user-visible OTA flow, endpoints, or backup semantics
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Extract shared reboot/health-check polling logic.
2. Extract shared Dallas label restore and redirect behavior.
3. Rewire both OTA entry points to use the shared helpers.
4. Preserve current strings, endpoints, and timeout behavior.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Extracted shared OTA health polling, Dallas label restore, and redirect helpers in updateServerHtml.h; rewired both update page scripts to use them while preserving endpoints and visible flow.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refactored the OTA page JavaScript in updateServerHtml.h to share reboot polling, Dallas label restore, and redirect logic between the update flow and success page without changing endpoints, timeouts, or visible OTA behavior.
<!-- SECTION:FINAL_SUMMARY:END -->
