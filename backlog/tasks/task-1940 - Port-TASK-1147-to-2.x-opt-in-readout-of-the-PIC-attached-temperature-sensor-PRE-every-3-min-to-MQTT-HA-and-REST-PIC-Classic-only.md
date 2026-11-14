---
id: TASK-1940
title: >-
  Port TASK-1147 to 2.x: opt-in readout of the PIC-attached temperature sensor
  (PR=E every 3 min) to MQTT, HA and REST, PIC Classic only
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T16:07:00Z'
updated_date: '2026-11-14T16:20:00Z'
labels:
  - pic
  - port
  - mqtt
dependencies: []
priority: medium
ordinal: 994000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Maintainer order (Robert, 2026-11-14): compare 1.x and 2.0.0 on reading the PIC and port what 2.0.0 lacks; it only applies to the PIC Classic variant, on OT-Direct no PIC settings are read (they are simply there). Comparison (95): every PIC function of otgw-1.x.x (d0a5d290e) exists in dev except the TASK-1147 sensor readout (pollPicTempSensor, picTempSensorHasData, state.picSettings.sTempReading, settings.otgw.bPicTempSensor / PICtempsensor, REST pictempsensor, otgw-pic/temperature_reading, HA sensor PIC_Temperature_Reading id 242 sub 0x08 announced on the first valid reading) and buildPicDownloadUrl, which dev has in another form (kPicUpdateUrlFmt, OTGW-Core.ino:6767). dev reads PR=E only as the return water temperature when PR=D is R (TASK-1314/1371, every 60 s). 1.x source: commit 7ee2344e2. Two PR=E users must share the one reply.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setting PIC temperature sensor (default off, settings.ini PICtempsensor, REST pictempsensor) exists on 2.x; with it on and a PIC Classic in gateway firmware 6.6+ the firmware sends PR=E every 3 min; with it off, or on OT-Direct, or with no PIC, no extra PR=E is sent (host test on the gate, red on current dev)
- [ ] #2 A valid PR: E=<value> is published to otgw-pic/temperature_reading and announced to HA as PIC_Temperature_Reading (temperature, C) on the first valid reading; '-' or garbage publishes nothing and announces nothing (ADR-094); the PR=D settings entity is untouched
- [ ] #3 The TASK-1314 return water path (D=R, 60 s) keeps working and both share one PR=E reply without a doubled poll (host test)
- [ ] #4 build green, evaluate --quick no new FAIL, bench on the Classic (.63) by 09: setting on shows the topic or no entity without a sensor, setting off sends no PR=E
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (95) comparison 1.x (origin/otgw-1.x.x d0a5d290e) vs dev: 36 PIC-related function names in 1.x; all present on dev except pollPicTempSensor, picTempSensorHasData (the TASK-1147 readout) and buildPicDownloadUrl (dev builds the same URL with kPicUpdateUrlFmt, OTGW-Core.ino:6767). The PR= settings readout cycle (15 letters, 3 s), copyPicSettingValue (picSettingValue.h, TASK-1430) and publishAllPICsettings are on dev; dev gates them on isPICEnabled() = state.pic.bAvailable, so OT-Direct reads no PIC settings, as Robert wants. Robert's scope (2026-11-14, in session): only for the PIC Classic; OT-Direct reads no PIC settings.

Port choices: setting in PICBootSection (settings.picBoot.bTempSensor) under the 1.x file key PICtempsensor, so a 1.x settings.ini carries over; REST pictempsensor in GET + knownSettings, updateSetting dispatch (ADR-051 triple). Faux id 239 (1.x used 242, which is OTGWhvacid on dev), Sensors device, flag 0x08 -> otgw-pic/temperature_reading, announced just in time like 241 (queueNonOTDiscoveryIds + skip in the bulk loop). PR=E shares the reply with TASK-1314: pollPicTempSensor sends nothing while picSensorTretPollActive(); the E block keeps feeding Tret for D=R and also publishes the reading when the setting is on. Validity via picParseSensorE (-40..130 C; 1.x used -60..150). Web UI: SET_META + en/nl/de/el label and hint ('PIC Classic only'); the Greek text is mine, unreviewed by a native speaker. Test: tests/test_pic_temp_sensor_port.py (real E block + real gate + source checks), not run yet (compile window).
<!-- SECTION:NOTES:END -->
