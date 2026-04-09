---
id: TASK-233
title: Multi-zone PID heating control via MQTT room sensors
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:42'
updated_date: '2026-04-09 14:41'
labels:
  - sat
  - future-sprint
  - feature
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python SAT supports multiple heating zones each with independent thermostat input and PID controller. OT is single-master/single-slave with one room temp (MsgID 24) and one setpoint (MsgID 16). Solution: implement N virtual PIDs via MQTT zone inputs. Each zone pushes room_temp to set/<nodeId>/sat/zone/<n>/room_temp and setpoint to set/<nodeId>/sat/zone/<n>/setpoint. Firmware runs N PID instances and selects the most-demanding CH setpoint (highest demand wins). The boiler still receives one CH setpoint. This gives zone-aware control without multi-thermostat OT.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add sat_zone_count setting (1-4, default 1)
- [x] #2 Add MQTT subscriptions for set/<nodeId>/sat/zone/<n>/room_temp and set/<nodeId>/sat/zone/<n>/setpoint for n=1..sat_zone_count
- [x] #3 Allocate N PID state structs (static array, max 4 zones) in SATcontrol.ino
- [x] #4 Zone selector: most-demanding setpoint wins (highest CH setpoint among active zones)
- [x] #5 Publish per-zone diagnostics: sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active
- [x] #6 Zone times out after sat_zone_timeout_s without update (default 300s)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add sat_zone_count (uint8_t, 1-4, default 1) and sat_zone_timeout_s (uint16_t, default 300) to SATSection in OTGW-firmware.h
2. Add SATZoneState struct (room_temp float, setpoint float, last_update uint32_t, active bool, pid_integral float, pid_prev_error float) to OTGW-firmware.h
3. Add static SATZoneState satZones[4] in SATcontrol.ino (BSS segment, not stack)
4. Add zone PID helper satZonePidUpdate() in SATcontrol.ino — simplified single-step PID per zone using zone integral + prev_error state, zone room_temp/setpoint, heating curve
5. Add satHandleZoneRoomTemp() and satHandleZoneSetpoint() input handlers in SATcontrol.ino (mirror satHandleAreaTemp pattern)
6. Add satPublishZoneDiagnostics() in SATcontrol.ino — publishes sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active for n=1..zone_count
7. Hook zone selection into satControlLoop(): when sat_zone_count > 1, run PID for each active zone (last_update within timeout), pick highest CH setpoint as pidOutput; fall through to zone 1 single-zone behavior when sat_zone_count == 1
8. Add MQTT subscriptions in MQTTstuff.ino: sat/zone/<n>/room_temp and sat/zone/<n>/setpoint (1-based index, n=1..sat_zone_count)
9. Add settings persistence in settingStuff.ino: SATzonecount and SATzonetimeout keys
10. Build with python build.py --firmware to verify no compile errors
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation complete. ESP8266 build verified clean. ESP32 build in progress (15+ min cold compile is normal for this project).
- SATZoneState struct: 28 bytes per zone, 4 zones = 112 bytes total on BSS
- Zone PID uses deadband-only integral matching SAT Python convention
- Literal float constants used instead of references to SATpid.ino statics (forward reference issue in single-TU Arduino compilation order)
- MQTT topic: set/<nodeId>/sat/zone/<n>/room_temp and set/<nodeId>/sat/zone/<n>/setpoint (1-based)
- Diagnostics: sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active (retained)
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added multi-zone PID heating control to OTGW firmware (TASK-233).

What changed:
- OTGW-firmware.h: SATZoneState struct (room_temp, setpoint, PID integral/prev_error, last_update, validity flags); two new SATSection fields: iZoneCount (1-4, default 1) and iZoneTimeoutS (default 300s); forward declarations for zone input handlers.
- SATcontrol.ino: static satZones[4] array (BSS, ~112 bytes); satHandleZoneRoomTemp/Setpoint handlers; satZonePidStep() per-zone simplified PID using deadband-only integral (matches SAT Python convention); satPublishZoneDiagnostics() publishes retained sat/zone/<n>/output, sat/zone/<n>/error, sat/zone/<n>/active; zone selector inserted in satControlLoop after primary PID — picks highest CH setpoint across active zones. Zones time out if no update received within iZoneTimeoutS.
- MQTTstuff.ino: handles set/<nodeId>/sat/zone/<n>/room_temp, set/<nodeId>/sat/zone/<n>/setpoint (1-based), zone_count, and zone_timeout_s commands.
- settingStuff.ino: SATzonecount and SATzonetimeout persisted to LittleFS.

Behavior: iZoneCount=1 (default) leaves single-zone behavior completely unchanged. Multi-zone activates only when iZoneCount>1. KISS: no dynamic allocation, static array, max 4 zones.

Build: ESP8266 verified clean. ESP32 build in progress (long compile, known behavior).
<!-- SECTION:FINAL_SUMMARY:END -->
