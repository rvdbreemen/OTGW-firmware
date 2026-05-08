---
id: TASK-584
title: 'feat(settings): persist ventilation overrides across reboots'
status: To Do
assignee: []
created_date: '2026-05-08 16:01'
labels:
  - otdirect
  - settings
  - ventilation
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OT-Thing-OTGW32 persists ventilation mode settings (ventEnable, openBypass, autoBypass, freeVentEnable, ventSetpoint) in its config.json. In 2.0.0, these are set via MQTT at runtime but are not saved to settings.ini -- they are lost on reboot. Users who control ventilation via OpenTherm must re-apply their settings after every reboot or firmware update. Add persistent storage for ventilation control fields in the OTDirect settings section.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 settings.otd gains fields: bVentEnable (bool), bOpenBypass (bool), bAutoBypass (bool), bFreeVentEnable (bool), iVentSetpoint (uint8_t, 0-100%)
- [ ] #2 Values are written to settings.ini on change (via writeSettings())
- [ ] #3 Values are restored from settings.ini at boot before OTDirect initialises
- [ ] #4 MQTT SET commands for these topics update both runtime state and persisted settings
- [ ] #5 REST API PATCH /api/v2/otdirect/settings accepts and persists these fields
- [ ] #6 Defaults: bVentEnable=false, bypass fields=false, iVentSetpoint=0 (no change from current boot behavior)
<!-- AC:END -->
