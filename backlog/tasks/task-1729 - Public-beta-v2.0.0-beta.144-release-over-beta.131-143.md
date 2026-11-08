---
id: TASK-1729
title: Public beta v2.0.0-beta.144 (release over beta.131-143)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-08T11:33:00Z'
updated_date: '2026-11-08T12:35:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 837000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08 ~12:20: public beta now. Candidate beta.143+6c7d1ca (code = origin/dev b5ed03e23), soak on .63 (Classic + S3 Mini, PIC) from 12:26. Release commit: notes docs/releases/RELEASE_NOTES_2.0.0-beta.144.md, CHANGELOG rollover, bump beta.144. Builds use UV_OFFLINE=1 during the GitHub incident of 2026-11-08 (unauthenticated timeouts).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Candidate ran 1 h on .63 with MQTT connected, no restart, no crashlog
- [x] #2 Release commit built (esp32-combo), OTA on .63 healthy
- [x] #3 Tag v2.0.0-beta.144 on the release commit; GitHub prerelease with --latest=false, assets incl. .elf and safeboot, SHA256SUMS match; latest stays the 1.x stable
- [x] #4 #alpha-testing zip post and #beta-testing announcement
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Soak 2026-11-08: beta.143+6c7d1ca on .63 (Classic + S3 Mini, PIC) OTA at 12:26 (settings diff only the intended TASK-1698 migration s0counterpin/gpiooutputspin); at 13:27 uptime 01:01, bootcount 170 (= 168 + flash 2), MQTT connected, crashlog none. Release commit 7ed8c7019 built 12:53 with UV_OFFLINE=1 (GitHub incident), 2x 'Successfully created ESP32S3 image', evaluate --quick 86/0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Candidate beta.143+6c7d1ca soaked 1 h on .63 (Classic + S3 Mini, PIC): uptime 01:01, bootcount 170 = flash + 2, MQTT connected, no crashlog. Release commit 7ed8c7019 (notes + CHANGELOG rollover + bump) built with UV_OFFLINE=1 during the GitHub incident, evaluate 86/0; OTA on .63: settings changed none, after 3 min bootcount 172 = +2, MQTT connected, no crashlog. Tag v2.0.0-beta.144 = 7ed8c7019. Prerelease published with --latest=false (latest stays v1.7.9); 7 assets incl. .elf, safeboot and SHA256SUMS, sizes equal to build/, downloaded flash zip sha256 c512174a... equals SHA256SUMS. Posts: #alpha-testing 1568966919858425941 (zip), #beta-testing 1568967023659196478 (1578 chars, read back). Not verified: soak of the release build itself beyond 3 min (code identical to the soaked candidate).
<!-- SECTION:FINAL_SUMMARY:END -->
