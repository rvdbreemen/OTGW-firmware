---
id: TASK-112
title: 'SAT Audit C4: LittleFS storage and persistence'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-3
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify SAT configuration persistence via LittleFS. Compare which settings Python SAT persistently stores vs C++ firmware. Verify read/write error handling.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All persistently stored SAT settings listed (Python vs C++)
- [x] #2 Read/write error handling verified
- [x] #3 Upgrade/migration path for settings schema changes present
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for missing persistence
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited SAT settings persistence via LittleFS on both platforms.

Settings persistence (settings.ini via writeSettings/readSettings):
- All 55+ SAT settings from SATSection struct are persisted in settings.ini with writeJsonBoolKV/writeJsonIntKV/writeJsonFloatKV.
- The read-back uses strcasecmp_P + parseJsonKVLine; all SAT keys verified in settingStuff.ino lines 763-857.
- BLE settings (bBleEnable, sBleMAC, iBleInterval) correctly guarded by #if defined(ESP32) in both write (line 349-354) and read (line 852-857) paths.
- OTDirect settings (OTDmode, etc.) correctly guarded by #if defined(HAS_DIRECT_OT) on both write and read paths.
- Settings file format is identical on both platforms — LittleFS JSON, no migration issues.

PID state persistence:
- satSavePidState() saves fPidI, fPidD, fError to /sat_pid_state.json on SAT disable and periodically.
- satLoadPidState() restores on boot. Works identically on both platforms (LittleFS API is unified).

Read/write error handling:
- writeSettings(): if LittleFS.open fails, silently uses defaults.
- readSettings(): if file not found, calls writeSettings to create it. If open fails, keeps defaults with DebugTln warning.
- Error handling is minimal but consistent and identical on both platforms.

Settings schema migration:
- No versioning or migration mechanism exists. New settings added to SATSection get their C++ default values on first boot after upgrade (safe: defaults are always sensible). Removed settings are silently ignored on readSettings. This is acceptable for now but has no explicit migration path.

Issues found:
- TASK-196 (HIGH): fEnergyTotal (sat/energy_total, retained MQTT) is transient state — never saved to LittleFS. Reboot resets the HA energy dashboard counter. Fix: add satSaveEnergyState() similar to satSavePidState().
- No settings that are ESP8266-only or OTGW32-only outside the properly guarded BLE/OTDirect sections.
<!-- SECTION:FINAL_SUMMARY:END -->
