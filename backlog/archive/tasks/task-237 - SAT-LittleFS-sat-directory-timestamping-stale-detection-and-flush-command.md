---
id: TASK-237
title: 'SAT LittleFS: /sat directory, timestamping, stale detection, and flush command'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 10:47'
updated_date: '2026-04-09 15:25'
labels:
  - sat
  - littlefs
  - persistence
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT persistent files are currently at root LittleFS (/sat_energy.json, /sat_pid_state.json, /sat_hcr.json etc). This task reorganizes them under /sat/, adds ISO timestamps to all stored datasets, implements stale detection per dataset type, auto-flushes on long offline periods, and adds a manual flush command via MQTT and REST API.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Create /sat/ directory on LittleFS; migrate all existing SAT files from root to /sat/
- [x] #2 Add timestamp field to all SAT persistence files (Unix epoch at write time)
- [x] #3 Stale detection strategy: PID integral stale after 30min, cycle window stale after 4h, HCR daily medians never stale (accumulate), energy totals never stale
- [x] #4 Auto-flush: when device has been offline > sat_flush_threshold_h (configurable, default 24h), clear short-lived datasets on next SAT enable
- [x] #5 Manual flush via MQTT set/<nodeId>/sat/flush and REST API POST /api/v2/sat/flush (clears all short-lived datasets, resets to clean state)
- [x] #6 Persist SATWindowRecord ring buffer and _hcr_samples to /sat/ for quick recovery on restart
- [x] #7 Document which datasets are persisted, their stale thresholds, and recovery behavior
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add iSatFlushThresholdH to SATSection in OTGW-firmware.h (default 24)
2. Add persist/load for iSatFlushThresholdH in settingStuff.ino (key SATflushtreshold)
3. Create satEnsureDir() helper; update all 5 SAT file path constants to /sat/sat_*.json; add migration on load
4. Add timestamp field (ts = Unix epoch) to satSavePidState, satSaveEnergyState, satSaveEstimatedEnergy, satHCRSaveState
5. Add stale detection on load: PID stale after 30min (already exists, just update path), cycle window stale after 4h, energy never stale, HCR never stale
6. Implement satFlushShortLivedData(): delete/reset PID integral and cycle window saves
7. Add cycle window (_win4h) persistence: satSaveCycleWindow() / satLoadCycleWindow() - capped at 60 entries, with 4h stale threshold
8. Add HCR intraday (_hcr_samples) persistence: save/load alongside satHCRSaveState/satHCRLoadState
9. Add MQTT handler for sat/flush in MQTTstuff.ino (calls satFlushShortLivedData)
10. Add REST handler for POST /api/v2/sat/flush in restAPI.ino handleSAT()
11. Add auto-flush on SAT enable: if saved > iSatFlushThresholdH hours ago, clear short-lived data
12. Save shutdown timestamp to /sat/sat_shutdown.json on satDisable()
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reorganized SAT LittleFS persistence under /sat/ directory with timestamps, stale detection, and flush commands.

Changes:
- OTGW-firmware.h: added iSatFlushThresholdH (default 24h) to SATSection
- settingStuff.ino: added SATflushtreshold persist/load (range 1-720 hours)
- SATcontrol.ino: added satEnsureDir(), satMigrateFile(), satSaveShutdownTs(), satReadShutdownTs(), satFlushShortLivedData(); updated all SAT file path constants to /sat/sat_*.json; added migration on load; updated initSAT() to call satEnsureDir(), check auto-flush threshold, load cycle window; updated satDisable() to save cycle window and shutdown timestamp; added periodic cycle window save (hourly) in control loop
- SATcycles.ino: updated SAT_HCR_FILE to /sat/sat_hcr.json with old-path migration; updated satHCRSaveState() to write ts + intraday samples (up to 96); updated satHCRLoadState() to restore intraday _hcr_samples; added satSaveCycleWindow(), satLoadCycleWindow(), satFlushCycleWindow() for _win4h ring buffer (capped at 60 records, 4h stale threshold)
- MQTTstuff.ino: added sat/flush and sat/flush_threshold_h subcommand handlers
- restAPI.ino: added POST /api/v2/sat/flush endpoint

Dataset persistence summary:
- /sat/sat_pid_state.json: PID integral, stale after 30min
- /sat/sat_cycles.json: cycle window ring buffer (up to 60 records), stale after 4h
- /sat/sat_hcr.json: HCR daily medians + intraday samples (up to 96), never stale
- /sat/sat_energy.json and sat_energy_estimate.json: energy totals, never stale
- /sat/sat_shutdown.json: shutdown timestamp for auto-flush detection

Migration: existing /sat_*.json root files are copied to /sat/ and deleted on first load.
<!-- SECTION:FINAL_SUMMARY:END -->
