---
id: TASK-975
title: 'v2: BLE scans by default + Clear-roster control in Sensors settings'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 08:42'
updated_date: '2026-07-01 09:05'
labels: []
dependencies: []
ordinal: 187000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-974 (BLE surfaced under Sensors). User: (1) BLE should scan + populate the roster by DEFAULT on a fresh install; (2) the settings page should let the user change the roster or CLEAR it. Changes: (a) firmware — settings default settings.sat.bBleEnable false->true (SATtypes.h:512). Passive-continuous scan, no-op on non-BLE boards (ESP8266 stub); existing installs keep their persisted value, only fresh installs / missing-key migrations get the default. (b) UI — renderBleCard: add a 'Clear roster' button (shown only when slots populated) that forgets every slot via POST /api/v2/sat/ble/forget (which also cleans HA discovery), two-click confirm (no blocking modal), then re-fetches. Change-the-roster (Select/Forget/label/bindkey/add-encrypted) already exists per-sensor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fresh install (no persisted settings) boots with BLE scanning enabled and the roster populates
- [x] #2 Sensors settings has a 'Clear roster' control that forgets all slots (HA discovery cleaned)
- [x] #3 esp32 firmware + fs build green; verified on .39: Clear roster empties the roster, 0 console errors
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
BLE scans by default + Clear-roster control (follow-up to TASK-974). Firmware: settings default bBleEnable false->true (SATtypes.h) — data/settings.ini has no satbleenable key so a fresh install uses the struct default; passive-continuous, no-op on non-BLE boards; existing installs keep their persisted value. UI: renderBleCard 'Clear roster' button (shown when slots populated), forgets every slot via POST /api/v2/sat/ble/forget (which also cleans HA discovery), fired SEQUENTIALLY (concurrent Promise.all tripped REST_MAX_INFLIGHT=4 -> some forgets 503), two-click confirm (no modal). Verified on .39: fresh settings.ini + reboot + zero POST -> satbleenable=true + roster auto-populated 3/4 sensors; Clear roster 4->0 with forget POSTs all 200 (was 200,200,503,503 before the sequential fix); heap_free 43KB/frag 27% healthy on the new firmware; 0 console errors.
<!-- SECTION:FINAL_SUMMARY:END -->
