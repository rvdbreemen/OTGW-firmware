---
id: TASK-1657
title: >-
  Public beta v2.0.0-beta.95 (beta.94 code + release commit; replaces the
  cancelled 08:00 beta.80)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T11:19:00Z'
updated_date: '2026-11-05T12:56:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 781000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-05 ~12:30: 'This morning you skipped the public beta, do it now'. Candidate beta.94 (c474756, benched on .61). Release commit beta.95 (docs-only): notes bundle beta.78..94 since v2.0.0-beta.77, CHANGELOG folds the never-published [2.0.0-beta.80] section, RELEASE_NOTES_2.0.0-beta.80.md replaced. Soak 1 h on the Classic .63 with MQTT before publishing.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes beta.95 checked against code, no em dashes/TODO, CHANGELOG rollover with the beta.80 section folded in
- [x] #2 esp32-combo build of the release commit green; OTA on .63 (Classic + PIC), settings unchanged apart from intended new keys
- [x] #3 1 h soak on .63 with MQTT: bootcount = flash + 2, no crashlog, heap not trending down
- [x] #4 Tag + GitHub prerelease (--latest=false), assets and SHA256SUMS verified, latest stays 1.x
- [x] #5 #alpha-testing zip post and #beta-testing announcement sent and read back
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.95 published 2026-11-05 ~13:55 (Robert: 'This morning you skipped the public beta, do it now'). Release commit 1fbd5cf43 (beta.94 code + notes bundling beta.78..94, the cancelled beta.80 section folded in). Build: 2x 'Successfully created ESP32S3 image', 'Build completed successfully'. OTA on .63 (Classic + PIC 6.8): settings changed only telnetenabled/wifinoportal (intended). Soak 1 h with MQTT: bootcount 145 = flash + 2 for the full hour (12:53:42 -> 13:53:40), mqtt=True throughout, crashlog empty, heap 56400 -> 55772 then flat, min free 20920 flat. Release: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.95, prerelease, latest stays v1.7.9, 7 assets equal to build/, downloaded zip SHA256 ffb2846a...c760c9 = SHA256SUMS. Discord: #alpha-testing zip msg 1567885102766104576, #beta-testing announcement msg 1567885180436484158 (read back intact). Soak sampler was reaped by Claude Code once for low memory; readings continued by single requests.
<!-- SECTION:FINAL_SUMMARY:END -->
