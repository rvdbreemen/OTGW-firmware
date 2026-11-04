---
id: TASK-974
title: >-
  v2: surface BLE sensors under Sensors (root cause: enable toggle buried under
  SAT)
status: Done
assignee: []
created_date: '2026-07-01 06:00'
updated_date: '2026-07-01 06:01'
labels: []
dependencies: []
ordinal: 186000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User report: BLE sensors never visible in v2 (async 2.0.0) despite 4 sensors next to the OTGW32; 'should work independently of SAT, they are sensors'. Systematic RCA on .39: (L3) roster endpoint /api/v2/sat/ble/discovery returned populated_slots:0/sensors:[] -> firmware found nothing, NOT a UI bug. (L1) settings showed satbleenable:false -> BLE disabled. satBLEInit() (SATble.ino:626) gates 'if(!bBleEnable) return' so the scan never starts. Enabling + reboot -> all 4 sensors found (A4:C1:38:xx Xiaomi/ATC). Proved firmware is CORRECT: satBLELoop() is called unconditionally in the main loop (OTGW-firmware.ino:983, NOT SAT-gated) and lazy-inits the scan at runtime; controlled experiment (boot BLE-off n=0 valid over 35s -> runtime enable -> 18s later n=4 fresh age<15s) confirms runtime enable works with NO reboot, independent of satenabled. TRUE root cause: the enable toggle + BLE config lived under the SAT settings category (cat:'sat'), so a user who thinks of BLE as sensors looks under Sensors, never finds it, never enables it. Fix (data-only): move satbleenable/satblefailover/satblemac/satbleinterval from cat:'sat' to cat:'sensors', relabel 'Enable BLE sensors', next to the existing roster card. No firmware change (firmware already independent of SAT + runtime-enables).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLE enable + config appear under the Sensors settings category, not SAT
- [x] #2 Enabling BLE in the web UI starts the scan at runtime (no reboot) and sensors appear in the roster, selectable
- [x] #3 Verified on .39: enable-under-Sensors -> 4 sensors discovered -> select works, 0 console errors
<!-- AC:END -->
