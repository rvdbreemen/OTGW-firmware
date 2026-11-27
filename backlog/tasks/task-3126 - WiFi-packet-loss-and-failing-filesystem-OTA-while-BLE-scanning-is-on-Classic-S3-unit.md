---
id: TASK-3126
title: >-
  WiFi packet loss and failing filesystem OTA while BLE scanning is on (Classic
  + S3 unit)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-27T13:39:00Z'
labels:
  - bug
  - ble
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1179000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench A/B by otgw-bench-tester on the Classic + S3 unit with extra memory (one unit, n=1 per state, router ping 12 ms, 2026-11-27): ping loss 28 % (average 425 ms) with BLE scanning on; 8 % right after setting satbleenable=false (BLE kept scanning until the reboot); 3 % (average 175 ms) after the reboot with BLE really off. The filesystem update over WiFi failed 5 of 5 times with BLE on and succeeded at the first try in about 20 s with BLE off (155 settings keys unchanged; the setting was restored afterwards, 155 of 155 equal). Evidence: OTGW-field-data/bench-63/2026-11-27_ble_ab/. Second finding: setting satbleenable=false did not stop the scan until a reboot. The UI switch is 'Enable BLE sensors' (Settings, Sensors, BLE sensors; v2.js:4504). Owner: otgw-dev. Relayed by the coordinator 2026-11-27. Not verified: only one unit, one run per state; no root cause yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on the same unit with n >= 3 runs per state (BLE on, BLE off after reboot): ping loss and the filesystem OTA result, numbers recorded in the task
- [ ] #2 Root cause named with file:line, one of: radio coexistence settings, BLE scan window and interval, the scan not paused during an OTA
- [ ] #3 Fix: for example pause the BLE scan during an OTA and lower the scan duty; host test where the code allows, red on the old code and green on the fix
- [ ] #4 Bench proof by otgw-bench-tester: filesystem OTA succeeds 5 of 5 with BLE on, and ping loss with BLE on is recorded before and after the fix
- [ ] #5 Separate finding: setting satbleenable=false stops the BLE scan without a reboot, or the UI and the docs state that it takes effect after a reboot (decision recorded)
<!-- AC:END -->
