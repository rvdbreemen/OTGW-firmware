---
id: TASK-1606
title: >-
  Public beta v2.0.0-beta.77 (20:00 slot, 2026-11-04): TASK-1605 crash fix +
  beta.63-76
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T17:45:00Z'
updated_date: '2026-11-04T19:14:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 747000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release of dev beta.76 + release commit as v2.0.0-beta.77 via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert's go recorded
- [x] #2 Release commit: bump beta.77, CHANGELOG rollover, release notes RELEASE_NOTES_2.0.0-beta.77.md
- [x] #3 esp32-combo build green, OTA on .63, 60 min MQTT soak: no reboot, no new crash, settings unchanged
- [x] #4 Tag v2.0.0-beta.77, GitHub prerelease (--latest=false), assets + SHA256SUMS verified by download
- [x] #5 #alpha-testing zip post and #beta-testing announcement (shown to Robert first)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 ~18:45 Robert (interactive): 'Go, 20:00 met soak'.

2026-11-04: build 03339b6 (2x 'Successfully created ESP32S3 image'); OTA .63 + .61, settings unchanged; soak .63 19:01-20:01: boot 125 throughout, MQTT up, dropped 0, heap 55.6-56.0 KB, settings identical (cmp), no reboot after the flash. Tag v2.0.0-beta.77 -> 03339b68d; prerelease (latest stays v1.7.9) with 7 assets incl. SHA256SUMS, sizes = build/; downloaded flash zip sha256 1e81d573... = SHA256SUMS. #alpha-testing zip msg 1567615113840689152 (posted from a worktree at the tag); #beta-testing announcement (Robert-approved text) msg 1567618038365294833, read back intact.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.77 published 2026-11-04 ~20:03: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.77 (TASK-1605 crash fix + beta.63-76). Evidence: Robert's go, 61 min MQTT soak on .63 clean, download SHA match, both Discord posts read back.
<!-- SECTION:FINAL_SUMMARY:END -->
