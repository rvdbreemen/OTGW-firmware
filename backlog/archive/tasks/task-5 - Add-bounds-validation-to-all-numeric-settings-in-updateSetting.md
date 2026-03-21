---
id: TASK-5
title: Add bounds validation to all numeric settings in updateSetting()
status: Done
assignee:
  - '@claude'
created_date: '2026-03-12 20:51'
updated_date: '2026-03-12 21:47'
labels:
  - bug
  - safety
  - settings
dependencies: []
references:
  - 'src/OTGW-firmware/settingStuff.ino:399-534'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
updateSetting() in settingStuff.ino accepts numeric values via atoi() without range validation, allowing invalid or dangerous values to be stored. This affects multiple settings that control hardware pins, network ports, and timer intervals.

Root cause: every numeric setting uses bare `atoi(newValue)` with no bounds check.

Affected settings (settingStuff.ino):
- Line 401: MQTTbrokerPort — accepts any int, should be 1–65535
- Line 437: MQTTinterval — cast to uint16_t without sign check; negative values wrap to large unsigned (e.g. -1 → 65535), effectively disabling MQTT publishing
- Line 460: ui_graphtimewindow — unbounded int, should be 1–1440 (24h max)
- Line 476/496/521: GPIOSENSORSpin / S0COUNTERpin / GPIOOUTPUTSpin — accept negative values that bypass conflict detection (ESP8266 valid pins: 0–16)
- Line 485/508: GPIOSENSORSinterval / S0COUNTERinterval — accept any int, very large values overflow timer math
- Line 504: S0COUNTERdebouncetime — unbounded
- Line 505: S0COUNTERpulsekw — unbounded (typical S0: 1–10000)
- Line 531: GPIOOUTPUTStriggerBit — should be constrained 0–15 (OT status bit range)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MQTTbrokerPort is constrained to 1–65535
- [ ] #2 MQTTinterval rejects negative input before uint16_t cast
- [ ] #3 GPIO pin settings reject values outside 0–16 range
- [ ] #4 Interval settings (sensor, S0) are constrained to 1–3600
- [ ] #5 S0COUNTERpulsekw is constrained to 1–10000
- [ ] #6 GPIOOUTPUTStriggerBit is constrained to 0–15
- [ ] #7 ui_graphtimewindow is constrained to 1–1440
- [ ] #8 All validations log a warning via DebugTf when rejecting invalid input
- [ ] #9 No functional change for values already within valid ranges
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added bounds validation to all numeric settings in updateSetting():
- MQTTbrokerPort: range check 1-65535
- MQTTinterval: range check 0-65535 before uint16_t cast
- ui_graphtimewindow: constrain(1, 1440)
- GPIOSENSORSpin, S0COUNTERpin, GPIOOUTPUTSpin: range check 0-16 with reject
- GPIOSENSORSinterval, S0COUNTERinterval: constrain(1, 3600)
- S0COUNTERdebouncetime: constrain(0, 1000)
- S0COUNTERpulsekw: constrain(1, 100000)
- GPIOOUTPUTStriggerBit: constrain(0, 15)

All invalid values are either clamped or rejected with a debug warning. Build passes.
<!-- SECTION:FINAL_SUMMARY:END -->
