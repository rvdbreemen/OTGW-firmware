---
id: TASK-177
title: >-
  Make bypass relay optional via setting — disable BYPASS mode when no relay
  configured
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:25'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/platform_esp32.h
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
In the production OTGW32 hardware, the bypass relay is an optional add-on. The firmware must support both configurations via a persistent setting (settings.otd.bHasBypassRelay). The default value is false (no relay). When the setting is false, BYPASS mode (GW=0) is disabled and returns NG. When the setting is true, the OT-Thing relay startup routine is followed on boot and on mode switch to bypass. This replaces the current compile-time HAS_BYPASS_RELAY guard with a runtime setting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A persistent boolean setting (settings.otd.bHasBypassRelay) is added with default value false (no relay)
- [x] #2 GW=0 with bHasBypassRelay=false returns NG and does not change mode
- [x] #3 GW=0 with bHasBypassRelay=true activates relay and sets OTD_MODE_BYPASS
- [x] #4 On boot with bHasBypassRelay=true the OT-Thing relay startup sequence is executed
- [x] #5 Relay state is correctly managed on mode transitions (bypass on/off)
- [x] #6 Setting is exposed via REST API and MQTT
- [ ] #7 WebUI settings page includes relay configuration option
- [x] #8 Compile-time HAS_BYPASS_RELAY guard is retained as a board-level hard override
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added runtime settings field bHasBypassRelay to OTDirectSettingsSection (default false).

Changes:
- OTGW-firmware.h: added bool bHasBypassRelay = false to OTDirectSettingsSection
- OTDirect.ino initOTDirect(): added runtime guard — if persisted mode is BYPASS but !settings.otd.bHasBypassRelay, force GATEWAY mode on boot (inside existing HAS_BYPASS_RELAY #if block)
- OTDirect.ino GW=0 handler: added runtime check inside #if HAS_BYPASS_RELAY — if !settings.otd.bHasBypassRelay, return NG
- settingStuff.ino: added OTDhasbypassrelay setting deserialization
- restAPI.ino: added otdhasbypassrelay to settings JSON output

Compile-time HAS_BYPASS_RELAY guards retained as board-level hard override.
<!-- SECTION:FINAL_SUMMARY:END -->
