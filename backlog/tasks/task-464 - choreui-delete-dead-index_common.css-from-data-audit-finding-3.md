---
id: TASK-464
title: 'chore(ui): delete dead index_common.css from data/ (audit finding 3)'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 23:35'
updated_date: '2026-04-27 23:42'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
index_common.css exists in src/OTGW-firmware/data/ but no HTML file links it (grep "href.*index_common" returns 0 hits in data/*.html). It's dead code wasting LittleFS space and confusing readers. Before deletion: grep its unique selectors against components.css to confirm coverage; any selector not yet present should be migrated. Then delete the file.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Confirmed no HTML file references index_common.css
- [x] #2 Unique rules from index_common.css verified to be covered in components.css OR migrated
- [x] #3 index_common.css deleted from data/
- [x] #4 Build clean and LittleFS image rebuilt
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
index_common.css deleted. Verified no HTML references via grep. Selectors covered by components.css. LittleFS image rebuilt clean.
<!-- SECTION:FINAL_SUMMARY:END -->
