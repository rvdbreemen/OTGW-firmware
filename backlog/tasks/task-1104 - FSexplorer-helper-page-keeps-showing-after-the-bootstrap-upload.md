---
id: TASK-1104
title: FSexplorer helper page keeps showing after the bootstrap upload
status: Done
assignee:
  - '@claude'
created_date: '2026-09-01 18:37'
updated_date: '2026-09-01 19:18'
labels:
  - audit
  - fsexplorer
dependencies: []
ordinal: 201000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The /FSexplorer and /FSexplorer.html routes are bound at boot to the result of an existence check, so a device that boots with an empty or partially written LittleFS keeps serving the 'you need to upload these files' helper even after the user has successfully uploaded FSexplorer.html through that very helper. Only a reboot clears it, which is exactly the situation the helper exists to rescue. Found by the FSexplorer audit of 2026-09-01, finding 14 of 18.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After uploading FSexplorer.html through the helper, the redirect serves the real explorer without a reboot
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The /FSexplorer and /FSexplorer.html routes are registered once with a per-request lambda that opens the file and falls back to the Helper page, replacing the boot-time existence latch. A bootstrap upload of FSexplorer.html now takes effect on the next GET without a reboot. Code-verified and built green; exercising it needs a device booted with an empty LittleFS, not reproducible on the provisioned bench device.
<!-- SECTION:FINAL_SUMMARY:END -->
