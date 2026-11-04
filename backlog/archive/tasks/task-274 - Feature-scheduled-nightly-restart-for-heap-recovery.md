---
id: TASK-274
title: 'Feature: scheduled nightly restart for heap recovery'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-15 19:58'
updated_date: '2026-04-15 22:01'
labels:
  - feature
  - stability
  - stap-3
dependencies: []
references:
  - src/OTGW-firmware/OTGW-firmware.h
  - src/OTGW-firmware/settingStuff.ino
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The ESP8266 heap fragments permanently after WebSocket connections due to lwIP 2.x MEMP_MEM_MALLOC architecture (root cause not fixable within core 3.1.2). A scheduled nightly restart resets the heap to a clean state and makes the fragmentation deterministically irrelevant.

Implementation: in doBackgroundTasks(), check once per minute if the current local time matches the configured restart window (default 04:00 local time). If heap is in a fragmented state (e.g. maxFreeBlock < threshold) AND we are in the restart window, schedule a clean restart. Alternatively: unconditional nightly restart at configured time.

Settings: add settings.system.bNightlyRestart (bool, default off) and settings.system.iRestartHour (int, 0-23, default 4). Expose in web UI settings panel and REST API.

The restart is clean: MQTT offline message is sent first, then ESP.restart(). The 30-second reconnect cycle is acceptable for a 4 AM maintenance window.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 settings.system.bNightlyRestart and settings.system.iRestartHour added to OTGWSettings struct and serialized to LittleFS
- [x] #2 Web UI settings panel exposes the nightly restart toggle and hour selector
- [x] #3 REST API /api/v2/settings reads and writes the new fields
- [x] #4 When enabled, device sends MQTT offline message and restarts cleanly at the configured hour (±1 minute)
- [x] #5 After restart, heap is fully recovered (logHeapStats shows max_block ~14KB within 60 seconds of boot)
- [x] #6 Feature is off by default (opt-in)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added nightly restart feature: settings.bNightlyRestart + settings.iRestartHour. Check in doTaskEvery60s() uses AceTime with configured timezone. Guards: uptime > 1 hour, NTP synced. Settings persisted via JSON. Feature is off by default.
<!-- SECTION:FINAL_SUMMARY:END -->
