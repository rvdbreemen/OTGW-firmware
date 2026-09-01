---
id: TASK-1104
title: FSexplorer helper page keeps showing after the bootstrap upload
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-01 18:37'
updated_date: '2026-09-01 18:41'
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
- [ ] #1 After uploading FSexplorer.html through the helper, the redirect serves the real explorer without a reboot
<!-- AC:END -->
