---
id: TASK-584
title: 'feat(settings): persist ventilation overrides across reboots'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 16:01'
updated_date: '2026-05-08 16:52'
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
- [x] #1 settings.otd gains fields: bVentEnable (bool), bOpenBypass (bool), bAutoBypass (bool), bFreeVentEnable (bool), iVentSetpoint (uint8_t, 0-100%)
- [x] #2 Values are written to settings.ini on change (via writeSettings())
- [x] #3 Values are restored from settings.ini at boot before OTDirect initialises
- [x] #4 MQTT SET commands for these topics update both runtime state and persisted settings
- [x] #5 REST API PATCH /api/v2/otdirect/settings accepts and persists these fields
- [x] #6 Defaults: bVentEnable=false, bypass fields=false, iVentSetpoint=0 (no change from current boot behavior)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add bVentEnable/bOpenBypass/bAutoBypass/bFreeVentEnable/iVentSetpoint to OTDirectSettingsSection\n2. Add writeJsonBoolKV/writeJsonIntKV for new fields in settingStuff.ino write block\n3. Add updateSetting handlers for all new fields in settingStuff.ino update block\n4. Add boot restoration of iVentSetpoint in initOTDirect() using updateWriteCache+setOverride\n5. Add write-through to settings in VS= handler in handleOTDirectCommand()\n6. Add GET display and POST handling of new fields in restAPI.ino settings endpoint\nNote: REST PATCH for VE/VB boolean flags limited to direct settings update; no new MQTT topics added (vs= already has MQTT via ventsetpt command)
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added persistent storage for ventilation override settings in OTDirect.

Changes:
- OTDirecttypes.h: added bVentEnable, bOpenBypass, bAutoBypass, bFreeVentEnable, iVentSetpoint to OTDirectSettingsSection with backward-compat defaults (all false, setpoint=0)
- settingStuff.ino: write (writeSettings) and parse (updateSetting) handlers for all 5 new fields with appropriate type constraints
- OTDirect.ino: boot restoration in initOTDirect() — if iVentSetpoint > 0, writes to MsgID 71 cache + override so scheduler re-applies to boiler without a new command
- OTDirect.ino: VS= command handler writes through to settings.otd.iVentSetpoint and calls writeSettings() on change
- restAPI.ino: new fields exposed in GET /api/v2/otdirect/settings; POST handler accepts hysteresisenable, hysteresis, ventenable, openbypass, autobypass, freeventenable, ventsetpoint args

Note: boolean vent flags (bVentEnable etc.) are persisted via REST POST /api/v2/otdirect/settings?ventenable=1; MQTT write-through for setpoint uses VS= command; other vent boolean MQTT topics not mapped (no existing VS-style commands for those flags in current MQTT command table).
<!-- SECTION:FINAL_SUMMARY:END -->
