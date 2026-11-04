---
id: TASK-963
title: >-
  v2 settings: BLE roster only under Sensors + working Rescan + continuous
  detection
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 22:53'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 175000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From user bug report + mockup 'Sensors unify 1-Wire + BLE roster'. (1) renderBleCard (v2.js:1443) appends to #setCols unconditionally (called at 1342 + async fetchBle 1435) -> BLE card shows on EVERY category. Gate inside renderBleCard on setActiveCat==='sensors'. (2) Rename category 'sensors' (v2.js:1019) 'GPIO Sensors'->'Sensors', desc include BLE. (3) Rescan button (v2.js:1452) gives no feedback -> looks dead; add Scanning... + disabled state + toast on click (POST already 200s). (4) Continuous detection: firmware is passive-continuous; poll /sat/ble/discovery (~4s) only while Sensors category open, re-render only on roster change (don't clobber bindkey input/focus); Rescan becomes optional nudge. Out of scope: live 1-Wire/Dallas readings + per-sensor room mapping.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLE roster card renders ONLY under the Sensors category (absent on System/MQTT/etc)
- [x] #2 Category renamed 'Sensors' (rail no longer shows 'GPIO Sensors')
- [x] #3 Rescan shows Scanning state + toast on click; roster auto-updates while Sensors open via discovery poll without clobbering inputs
- [x] #4 buildfs + 3-target build green; evaluate clean; verified on .39 (Playwright: card on Sensors only, Rescan POST fires + button state changes)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the v2 settings BLE roster. (1) renderBleCard (v2.js) appended to #setCols unconditionally (called at 1342 + async fetchBle) so the BLE card leaked onto EVERY category — gated it on setActiveCat==='sensors' (covers both call sites; stops the poll off-Sensors). (2) Renamed category 'GPIO Sensors' -> 'Sensors' (desc '1-Wire / GPIO temperature sensors and the BLE roster'). (3) Rescan gave no visible feedback -> looked dead; now shows '⏳ Scanning…' + disabled + a toast on click (POST already 200s), re-enables after 3s. (4) Continuous detection: firmware is passive-continuous (ADR-151); fetchBle now diffs the roster and re-renders only on change AND never while a card input is focused (won't clobber a half-typed bindkey); a 4s poll runs only while Sensors is open (self-stops on leave). Verified live on .39 (Playwright + screenshot): BLE card absent on System, present under Sensors; rail shows 'Sensors'; Rescan -> '⏳ Scanning…' disabled + POST 200; 0 console errors.
<!-- SECTION:FINAL_SUMMARY:END -->
