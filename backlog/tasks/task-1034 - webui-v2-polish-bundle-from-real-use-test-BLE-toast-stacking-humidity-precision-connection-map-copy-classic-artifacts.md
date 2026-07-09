---
id: TASK-1034
title: >-
  webui v2: polish bundle from real-use test - BLE toast stacking, humidity
  precision, connection-map copy, classic artifacts
status: Done
assignee:
  - '@claude'
created_date: '2026-07-09 18:11'
updated_date: '2026-07-09 18:33'
labels: []
dependencies: []
ordinal: 243000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the same 2026-07-09 real-use test on the S3 Mini Pro: (a) BLE 'New sensor discovered' toasts stack 4-high, never auto-dismiss, and cover content on EVERY page/tab until Assign/Ignore/x is clicked per toast - they should auto-collapse into one grouped toast or a badge and auto-dismiss after a timeout (roster page remains the place to act); (b) Sensors>BLE roster shows raw float humidity '57.950001%' - round to 1 decimal or integer; (c) Monitor>Connection boiler card shows contradictory copy: 'The boiler (OT slave) is answering.' directly above 'Boiler not answering - check the OT wiring / boiler power.' - the static explainer line should adapt to (or be removed for) the actual state; (d) classic UI keeps literal 'Wait for it...' text visible top-left after full load. All cosmetic/UX, no functional breakage; everything else in the walkthrough (Home system view, Monitor tabs, Settings all 13 sections, Advanced Debug/System, dark/light) rendered and behaved correctly with zero JS exceptions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLE discovery toasts group/auto-dismiss and no longer permanently cover page content
- [x] #2 Humidity values render with sane precision everywhere they appear
- [x] #3 Connection-map boiler/thermostat cards show state-consistent copy
- [x] #4 Classic 'Wait for it...' placeholder clears after load
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Grouped BLE discovery toasts (single card, N-count, View->Sensors roster, 15s auto-dismiss), humidity rounded to 1 decimal in roster (toast already rounded), connection-map healthy-copy now omitted on down/warn state (uniform for all nodes), classic Wait-for-it placeholder cleared on first devinfo. Verified on-device: SAT/Advanced/FS pages clean, no toast pile-up, no JS errors.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2/classic UX polish from the real-use test: grouped auto-dismissing BLE discovery toasts, sane humidity precision, state-consistent connection-map copy, and the classic 'Wait for it...' artifact cleared.
<!-- SECTION:FINAL_SUMMARY:END -->
