---
id: TASK-15
title: Refactor OTA updater flow
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-15 22:41'
updated_date: '2026-03-15 23:03'
labels:
  - refactor
  - ota
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Refactor the post-v1.2.0 OTA updater implementation without changing user-visible behavior. Focus on reducing duplication in the update page JavaScript, separating responsibilities in the OTA backend upload handler, and clarifying flash-mode runtime gating.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTA update page JS has a single shared reboot/health-check flow with no duplicated polling logic
- [x] #2 OTA backend upload handler is split into smaller helpers while preserving current OTA behavior
- [x] #3 Flash-mode runtime handling is clarified and extracted without changing ESP/PIC flash semantics
- [x] #4 Firmware builds successfully after the refactor
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Refactor the OTA page JavaScript to remove duplicated reboot/health polling and Dallas label restore logic while keeping the same UI behavior.
2. Refactor the OTA backend upload handler into smaller helpers without changing flash semantics.
3. Extract flash-mode runtime helpers in OTGW-firmware.ino to clarify ESP vs PIC flash task handling.
4. Build the firmware and do a quick sanity review of the touched OTA paths.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Completed OTA refactor across updateServerHtml.h, OTGW-ModUpdateServer, and OTGW-firmware.ino. Final firmware build passed after cleaning a generated version.h conflict state unrelated to the refactor itself.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed the OTA updater refactor without changing shipped behavior. The update page JavaScript now uses a single shared reboot/health-check flow inside the PROGMEM HTML, the OTA backend upload lifecycle is split into smaller helpers, flash-mode background handling is extracted into named helpers, and the firmware build now passes successfully.
<!-- SECTION:FINAL_SUMMARY:END -->
