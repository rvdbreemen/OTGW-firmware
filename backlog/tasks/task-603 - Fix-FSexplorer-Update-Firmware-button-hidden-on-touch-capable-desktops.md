---
id: TASK-603
title: Fix FSexplorer Update Firmware button hidden on touch-capable desktops
status: Done
assignee:
  - '@copilot'
created_date: '2026-05-15 16:15'
updated_date: '2026-05-15 16:21'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Update Firmware button in FSexplorer is hidden when (pointer: coarse) and (hover: none), which incorrectly affects devices like Surface Pro. Make the button visible on desktop-class layouts while keeping mobile behavior explicit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 FSexplorer Update Firmware action remains visible on desktop-class viewports even when device reports coarse pointer
- [x] #2 Touch-only mobile layouts still hide the update action when intended
- [x] #3 FSexplorer light and dark theme styles stay consistent
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-15: Updated FSexplorer UA/class gating so hide-fw-update applies to iPad/iPhone and Android phones; kept fallback media query for small touch-only layouts while preserving Surface desktop visibility.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented targeted UI fix in FSexplorer light/dark CSS and HTML bootstrap script. Update Firmware button now remains available on Microsoft Surface-class desktop usage, while being explicitly hidden for iPad/iPhone and Android smartphone user agents. Validation: reran evaluate.py --quick (same pre-existing warnings/failure), build.py still blocked by arduino-cli download DNS/network restriction in this environment; captured UI screenshots in /tmp for desktop, iPhone UA, and Android-phone UA.
<!-- SECTION:FINAL_SUMMARY:END -->
