---
id: TASK-1732
title: >-
  Web UI: no 'Settings saved' toast when switching BLE on waits for the
  stability warning
status: To Do
assignee: []
created_date: '2026-11-08T15:11:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 839000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the TASK-1693 bench (.61, beta.148): when satbleenable is the only dirty key on a board without PSRAM, saveSettings() removes it to show the stability warning, posts nothing, and still shows the green 'Settings saved' toast. Cosmetic but misleading. Show no toast (or a neutral one) when nothing was posted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With only satbleenable dirty on a no-PSRAM board without ack, Save shows the warning and no 'Settings saved' toast; a browser test is red on beta.148 and green on the fix
<!-- AC:END -->
