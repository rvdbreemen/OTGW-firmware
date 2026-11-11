---
id: TASK-1794
title: 'Release: 2.0.0-beta.174 public beta notes over beta.171-174'
status: Done
assignee: []
created_date: '2026-11-11T05:17:00Z'
updated_date: '2026-11-11T07:07:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 889000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta in the 08:00 slot of 2026-11-11 via /beta-release-2x (invoked by Robert). Candidate beta.174 (bceb92c86), soaked on .63 (Classic + S3 Mini) since 00:28 with MQTT connected since 03:59. Docs-only release commit on top (beta.170 model, no bump).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes + CHANGELOG + README committed and checked (no TODO/em dash/relative links)
- [x] #2 Release commit built (2 SUCCESS lines, fresh artifacts) and OTA bench-checked on .63: fwversion, settings unchanged, bootcount stable >= 5 min, no crashlog
- [x] #3 Tag v2.0.0-beta.174 pushed on the release commit; GitHub prerelease published with --latest=false; latest stays v1.7.9; download SHA matches
- [x] #4 #alpha-testing zip post and #beta-testing announcement sent and read back
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: release notes docs/releases/RELEASE_NOTES_2.0.0-beta.174.md + CHANGELOG [2.0.0-beta.174] + README links in e6b40377a; lint 0 TODO/TBD, 0 em dash, 0 relative links; evaluate --quick 0 failed. AC2: build of e6b40377a: 2x 'Successfully created ESP32S3 image' + 'Build completed successfully', artifacts 06:31-06:32; OTA on .63 (Classic + S3 Mini) 06:33: fwversion 2.0.0-beta.174+e6b4037, version.hash e6b4037, settings 150 keys changed none; 06:40 bootcount 11 (9+2), mqttconnected true, crashlog only the known 2026-11-10 06:43 ipc0 entry (TASK-1763). Candidate soak: beta.174+bceb92c on .63 since 00:28, MQTT connected from 03:59 (LAN broker had been down). AC3: tag v2.0.0-beta.174 -> e6b40377a; release published 08:01 isDraft false, prerelease true, --latest=false, releases/latest still v1.7.9; 7 assets sizes equal build/; downloaded flash zip sha256 ac0aaab1...c911cee equals SHA256SUMS. AC4: #beta-testing announcement msg 1569970970435788880 (read back, identical, 1522 chars); #alpha-testing: the zip post script REFUSED ('prerelease 2.0.0-beta.174 was already published') because Phase 6.1 ran after Phase 5; not worked around; a text note with the release link was posted instead (msg 1569971052573106266). Follow-up: skill order fix filed (see notes). Not tested: beta.172-174 on the OT-Direct bench unit (.61 stayed on 171).
<!-- SECTION:FINAL_SUMMARY:END -->
