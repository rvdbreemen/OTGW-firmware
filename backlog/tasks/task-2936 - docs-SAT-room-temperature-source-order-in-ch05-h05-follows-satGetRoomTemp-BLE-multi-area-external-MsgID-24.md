---
id: TASK-2936
title: >-
  docs: SAT room temperature source order in ch05/h05 follows satGetRoomTemp()
  (BLE, multi-area, external, MsgID 24)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T22:57:00Z'
updated_date: '2026-11-19T23:04:00Z'
labels:
  - docs
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 1068000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 92 while answering will_85903 (#beta-testing 1573109426934644797): ch05:36 and h05:40-45 listed MsgID 24 first; the code (SATcontrol.ino:2727-2771) uses BLE first, then multi-area, then the external MQTT/REST push (satexternaltemp, satsensormaxage), then MsgID 24. Docs only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch05/h05 list the room temperature sources in the order of satGetRoomTemp() with their stale rules, cited file:line
- [x] #2 MANUAL.md rebuilt, build_manual --check and test_manual_* green; pushed via the coordinator's slot (not in the 08:00 beta)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 00:02: checked on origin (not on 92's word alone): satGetRoomTemp() SATcontrol.ino:2727-2771: simulation first (bSimulation, test only), then BLE (bBleEnable && bBleTempValid, stale after SAT_STALE_TEMP_BLE_MS 300000 ms, :146), then multi-area (bMultiArea && count > 0 and a valid weighted value), then the external push (bUseExternalTemp = setting SATexternaltemp settingStuff.ino:1192, stale after iSensorMaxAgeS = SATsensormaxage :1292, default 6 h per the code comment), set by satHandleExternalTemp() :3183-3195 for MQTT and REST, then OT MsgID 24 (stale after SAT_STALE_TEMP_MS 300000 ms once a thermostat reported it, :147, :2766-2768). ch05:36 and h05:40-45 rewritten in that order. Gates: build_manual --check exit 0, test_manual_* 10 passed, no em dashes.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: ch05:36 and h05:40-45 list the room temperature sources in the order of satGetRoomTemp() (SATcontrol.ino:2727-2771): BLE (stale 5 min, :146), multi-area (when on and a valid area), external MQTT/REST push (satexternaltemp, stale after satsensormaxage, settingStuff.ino:1192/:1292; setter satHandleExternalTemp :3183-3195), MsgID 24 (stale 5 min after a thermostat reading, :147, :2766-2768). The text describes the firmware only, no thermo-nova claim. AC2: MANUAL.md rebuilt, build_manual --check exit 0, test_manual_* 10 passed; pushed on 046e41e22 in the coordinator's slot.
<!-- SECTION:FINAL_SUMMARY:END -->
