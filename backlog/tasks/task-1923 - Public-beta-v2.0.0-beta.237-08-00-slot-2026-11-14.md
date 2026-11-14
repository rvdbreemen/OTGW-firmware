---
id: TASK-1923
title: Public beta v2.0.0-beta.237 (08:00 slot 2026-11-14)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T07:06:00Z'
updated_date: '2026-11-14T07:57:00Z'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 985000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Publish the soaked beta.237+c66c9b9 as public prerelease v2.0.0-beta.237 via /beta-release-2x: docs-only release commit (notes bundling beta.225-237, CHANGELOG rollover, README), build, bench on .63, tag, draft, publish, #alpha-testing zip, #beta-testing announcement.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG rollover and README committed (docs-only)
- [x] #2 esp32-combo build of the release commit OK with fresh artifacts
- [x] #3 Bench OTA on .63: fwversion, settings diff none, no new crash, bootcount stable 5 min
- [x] #4 Tag v2.0.0-beta.237 on the release commit, prerelease published with --latest=false, assets SHA match
- [x] #5 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: docs-only release commit c36a0b613 (notes bundling beta.225-237, CHANGELOG rollover, README); evaluate --quick 0 failed. AC2: esp32-combo build 08:30-08:42, 'Build completed successfully', 2x 'Successfully created ESP32S3 image', artifacts 08:41-42 (an earlier attempt at 08:15 was reaped for low memory). AC3: 09 bench on .63: ota_flash app+FS exit 0, fwversion 2.0.0-beta.237+c36a0b6, settings 150 keys changed none, raw settings.ini diff none, bootcount 37->39 (flash+2) stable after 5 min, MQTT connected, no new crashlog; /update user route in Chrome OK. Soak of the candidate c66c9b9: 07:03-08:04 on .63, bootcount 37 stable. AC4: tag v2.0.0-beta.237 -> c36a0b6, prerelease published, --latest=false (latest stays v1.7.9), downloaded flash zip sha256 5f4977701d88abb9 = SHA256SUMS; release body trimmed to 98508 chars (older sections linked to the full notes at the tag). AC5: #beta-testing 1571071355003338842, #alpha-testing note 1571071356986982450 (zip already posted as alpha 1571041876306432021; post script refuses a re-post of a published tag). Not tested: 24 h soak, macOS capture (TASK-1544 waiver).
<!-- SECTION:FINAL_SUMMARY:END -->
