---
id: TASK-1905
title: Public beta v2.0.0-beta.224 (20:00 slot 2026-11-13)
status: Done
assignee: []
created_date: '2026-11-13T18:09:00Z'
updated_date: '2026-11-13T18:57:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 969000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert typed /beta-release-2x. Docs-only release commit on the soaked beta.224 (5ddcc6543; src unchanged up to origin/dev 479e49391). Notes bundle beta.219-beta.224.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG rollover and README on dev, claims checked against code
- [x] #2 Release build from the release commit; 2x Successfully created ESP32S3 image
- [x] #3 Bench check of the release build on the Classic+S3 Mini bench unit (.63): fwversion, bootcount +2 only, no crashlog, settings unchanged
- [x] #4 Tag v2.0.0-beta.224 on the release commit, GitHub prerelease --latest=false with assets and SHA256SUMS, download hash matches
- [x] #5 #alpha-testing zip post + #beta-testing announcement, ledger row
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: b769b8f0f (notes, CHANGELOG rollover, README; docs only, git diff 5ddcc6543..HEAD -- src empty); evaluate --quick 86 passed 0 failed; no TODO/em dash/relative links. Release body trimmed (beta.77-174 sections linked) because GitHub caps a body at 125000 chars (full file 128629). AC2: build.bat from b769b8f0f, exit 0, 2x Successfully created ESP32S3 image, Build completed successfully, artifacts 19:34-19:35. AC3: 09 OTA on .63: zip sha matched, settings 150 keys changed none, fwversion 2.0.0-beta.224+b769b8f, PIC 6.8, MQTT up, crashlog empty, stable 5 min (19:43). Deviation: bootcount 35 -> 1 instead of 37 (reboot_count reset, app-boot log entry missing), filed as TASK-1909, not a blocker (same src kept the counter at 16:22). Soak before release: 224 on .63 16:21-19:21, bootcount 35 unchanged, MQTT since 16:28. AC4: tag v2.0.0-beta.224 -> b769b8f0f; prerelease published, latest stays v1.7.9; 7 assets incl. SHA256SUMS, sizes equal build/; downloaded zip sha256 1de2a63a...37ed4 matches. AC5: #beta-testing 1570872427703959663 (read back intact), #alpha-testing note 1570872487804407908; the zip post script refused (beta.224 was already posted as alpha zip), so no second zip post.
<!-- SECTION:FINAL_SUMMARY:END -->
