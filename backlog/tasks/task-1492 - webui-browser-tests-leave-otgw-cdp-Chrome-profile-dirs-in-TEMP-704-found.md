---
id: TASK-1492
title: webui browser tests leave otgw-cdp-* Chrome profile dirs in %TEMP% (704 found)
status: To Do
assignee: []
created_date: '2026-10-29T22:40:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 651000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-firmware-d5 (2026-10-29): the tests call rmSync on the Chrome profile dir right after chrome.kill(), while Chrome still holds the directory, so the removal fails silently and the dirs pile up (704 in %TEMP%). Disk only, pre-existing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After a full tests/webui run no new otgw-cdp-* dir remains (wait for the Chrome process exit, or retry rmSync)
- [ ] #2 A one-off cleanup of the existing leftovers is documented or scripted
<!-- AC:END -->
