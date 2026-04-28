---
id: TASK-471
title: flash-upgrade-route-with-backup-restore
status: Done
assignee:
  - '@codex'
created_date: '2026-04-28 17:23'
updated_date: '2026-04-28 17:29'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add an upgrade path that can optionally back up settings and Dallas labels from a live OTGW before flashing, then restore them after the upgrade. The tool should ask whether to preserve settings, prompt for the device hostname, and report progress step by step.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The upgrade flow can back up settings and Dallas labels from a reachable OTGW before flashing.
- [x] #2 The upgrade flow restores backed-up data after flashing when preservation is enabled.
- [x] #3 The script prompts for the device hostname and reports each major step during the process.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented cross-platform live backup/restore path for full-image upgrades: prompt for preserving settings, prompt for OTGW host/IP, download /settings.ini and /api/v2/sensors/labels before flashing, wait for health after flash, restore via /upload, trigger reboot, wait again.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented a cross-platform upgrade route for flash_otgw.sh and flash_otgw.bat. The scripts now prompt for a live OTGW hostname, optionally back up settings.ini and Dallas labels before a full-image upgrade, flash the device, wait for health, restore the backups via /upload, trigger a reboot, and wait again before reporting completion.
<!-- SECTION:FINAL_SUMMARY:END -->
