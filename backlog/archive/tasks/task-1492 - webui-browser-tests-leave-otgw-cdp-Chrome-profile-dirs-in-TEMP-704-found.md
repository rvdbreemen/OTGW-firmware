---
id: TASK-1492
title: webui browser tests leave otgw-cdp-* Chrome profile dirs in %TEMP% (704 found)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T22:40:00Z'
updated_date: '2026-10-31T23:11:00Z'
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
- [x] #1 After a full tests/webui run no new otgw-cdp-* dir remains (wait for the Chrome process exit, or retry rmSync)
- [x] #2 A one-off cleanup of the existing leftovers is documented or scripted
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The 12 Chrome-based web UI tests wait for the Chrome process to exit (max 5 s) and remove the profile with fs.rmSync maxRetries 10 / retryDelay 200; a failed removal is printed instead of ignored. scripts/cleanup_cdp_profiles.py removes leftover %TEMP%/otgw-cdp-* dirs older than 60 min (dry-run option).
Evidence: count of otgw-cdp-* dirs before/after running all 12 tests on the new code: 1095 -> 1095 (no leak); the old v2-navigation test run 3 times: 1095 -> 1098. Cleanup run: see the script output in this commit's session (removed the old leftovers).
<!-- SECTION:FINAL_SUMMARY:END -->
