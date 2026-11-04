---
id: TASK-996
title: >-
  fix(v2-webui): BLE sensors cannot be named/renamed — add name input to the
  roster row
status: Done
assignee:
  - '@claude'
created_date: '2026-07-03 22:32'
updated_date: '2026-07-06 05:10'
labels: []
dependencies: []
ordinal: 208000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The v2 BLE roster (renderBleCard) exposes Select/Forget/bindkey but NO name field, so sensors stay '(unnamed)' and cannot be renamed. Firmware already supports it: POST /api/v2/sat/ble/label {mac,label} -> satBLERosterSetLabel, persisted sBleLabel[], emitted as 'label' in discovery JSON. Add a per-row name input (pre-filled with label, Enter/Save applies, empty clears).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each roster row has a name input pre-filled with the current label
- [x] #2 Save/Enter POSTs /sat/ble/label and the name persists across reloads
- [x] #3 Clearing the field and saving clears the label
- [x] #4 Verified on-device with evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Re-verified 2026-07-06 on 192.168.88.64 (alpha.330), code review + live evidence (implementation notes had never been recorded despite the ACs being checked, so this fills that gap). Confirmed real per-row name input exists at v2.js:2015-2029 (POST /api/v2/sat/ble/label, pre-filled with s.label, Enter/Save applies). Live test via CDP-driven browser: navigated Settings > Sensors, typed 'Living Room Sensor' into the first BLE roster row's name input, clicked Save name -> toast 'Name saved' -> GET /api/v2/sat/ble/roster confirmed label='Living Room Sensor' persisted (screenshot: docs/evidence/task996_02_ble_roster_after_save.png). Cleared it back (POST label='') -> GET confirmed label='' (AC#3). All 4 ACs hold with fresh evidence.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 BLE roster rows now have a per-row name input (pre-filled with the persisted label), Save/Enter posts to /api/v2/sat/ble/label, and clearing+saving removes the label. Verified live on 192.168.88.64: named a real discovered sensor 'Living Room Sensor' via the actual UI, confirmed persistence via GET, then cleared it back. Screenshot evidence in docs/evidence/.
<!-- SECTION:FINAL_SUMMARY:END -->
