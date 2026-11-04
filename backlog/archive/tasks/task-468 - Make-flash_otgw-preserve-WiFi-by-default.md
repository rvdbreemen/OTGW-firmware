---
id: TASK-468
title: Make-flash_otgw-preserve-WiFi-by-default
status: Done
assignee:
  - '@codex'
created_date: '2026-04-28 16:39'
updated_date: '2026-04-28 16:43'
labels: []
dependencies: []
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 flash_otgw.sh and flash_otgw.bat no longer require typing YES before flashing.
- [x] #2 An explicit destructive wipe path remains available for factory resets.
- [x] #3 Default flashing preserves existing WiFi/settings instead of replacing them with the merged-full filesystem image.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Removed the YES confirmation path from both flash_otgw wrappers. Default mode now selects firmware-only binaries so WiFi/settings are preserved, and --factory is the explicit full-image path. Verified flash_otgw.bat --help prints the updated mode text without prompting.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the interactive YES gate from both flash_otgw wrappers, switched the default flash mode to firmware-only so WiFi/settings are preserved, and added an explicit --factory path for full-image flashes. Verified the Windows wrapper help output reflects the new modes.
<!-- SECTION:FINAL_SUMMARY:END -->
