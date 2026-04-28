---
id: TASK-469
title: auto-detect-fresh-vs-flashed
status: Done
assignee: []
created_date: '2026-04-28 16:59'
updated_date: '2026-04-28 17:12'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Teach the flashing tooling to distinguish a blank/new ESP from a previously flashed one so it can choose factory-install versus upgrade behavior automatically. The goal is to keep first-time installs simple while preserving existing WiFi/settings on already flashed devices.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Flash tooling selects the factory/full-image path for fresh devices and the upgrade/firmware-only path for already flashed devices.
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added an interactive flash-mode chooser to flash_otgw.sh and flash_otgw.bat. Both scripts now probe the target flash, recommend factory reset for blank devices and upgrade for already-flashed devices, and still allow explicit factory / erase / firmware-only overrides.
<!-- SECTION:FINAL_SUMMARY:END -->
