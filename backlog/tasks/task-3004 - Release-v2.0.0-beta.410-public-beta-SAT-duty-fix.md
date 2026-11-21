---
id: TASK-3004
title: 'Release v2.0.0-beta.410 (public beta: SAT duty fix)'
status: Done
assignee: []
created_date: '2026-11-21T12:54:00Z'
updated_date: '2026-11-21T14:19:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1126000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta on Robert's order (#alpha-testing 1573682252607397969: Publish beta 410 now). Docs-only release commit on bce9496a5 + notes/CHANGELOG/README; tag, build, 1 h soak on .63, GitHub prerelease --latest=false, Discord posts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit pushed to dev
- [x] #2 Release build soaked 1 h on .63 (bootcount, no crashlog, MQTT)
- [x] #3 Tag + GitHub prerelease with verified assets (SHA256)
- [x] #4 #alpha-testing note and #beta-testing post
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (92): #beta-testing announcement of v2.0.0-beta.410 posted in 2 parts: 1573703875834286180 (1415 chars) and 1573703884793446487 (1038 chars), both read back equal; test-run line from the coordinator verbatim. Q-342b to Sergeant D followed (1573703896008892611 / 1573703904909328416).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: release commit c6f0302f4 (docs-only on bce9496a5) pushed to dev. AC2: release build beta.410+c6f0302 soaked on .63 14:12:21-15:14:14 (62 min): bootcount 363 = flash+2 throughout, crashlog none, 0 panic/wdt lines, MQTT 372/372, freeheap flat 35136->35088, maxfreeblock 21492 flat; evidence OTGW-field-data/bench-63/2026-11-21_beta410-c6f0302-soak. AC3: tag v2.0.0-beta.410 -> c6f0302f4; GitHub prerelease --latest=false, 7 assets sizes = build, latest stays v1.7.10, downloaded flash zip sha256 abd648d5... = build. AC4: #alpha-testing note 1573703749052928000; #beta-testing 1573703875834286180 + 1573703884793446487 (BlueFlame). Not verified: the SAT duty change on a real boiler (simulation only).
<!-- SECTION:FINAL_SUMMARY:END -->
