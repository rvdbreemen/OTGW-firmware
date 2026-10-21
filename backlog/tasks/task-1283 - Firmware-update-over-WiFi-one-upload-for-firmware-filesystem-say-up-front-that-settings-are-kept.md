---
id: TASK-1283
title: >-
  Firmware update over WiFi: one upload for firmware + filesystem, say up front
  that settings are kept
status: To Do
assignee: []
created_date: '2026-10-21 18:10'
labels:
  - ui
  - ota
  - update
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 412000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feedback gruijter 2026-10-21 (#alpha-testing). Today an update is two uploads (firmware through the safeboot updater, then the LittleFS image), settings survive both (TASK-1264: 0/567 changed), but that is not obvious. Confusing: after the firmware step and before the filesystem step the UI offers to save the settings to the laptop, which suggests they could be lost. The filesystem always ships with a firmware release, so one file upload would be easier. Backup/restore of settings is welcome but should stay out of the update flow. Explain before the update that settings are kept. Constraint: the main firmware cannot write ota_0 itself (ADR-191 safeboot), so a single upload needs a flow across the safeboot restart (e.g. upload the flash zip or both images once, safeboot writes the app, the main firmware then writes the filesystem) or a guided two-step page.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design recorded: single upload (which file, how the filesystem step crosses the safeboot restart) or guided flow; ADR if the OTA contract changes
- [ ] #2 The update page states before the update that settings are kept, and the mid-update 'save settings' prompt is gone or explained
- [ ] #3 Bench: an update from the previous alpha with one user action keeps settings (diff 0) and ends on the new firmware + filesystem
- [ ] #4 Screenshots of the flow
<!-- AC:END -->
