---
id: TASK-1940
title: >-
  Port TASK-1147 to 2.x: opt-in readout of the PIC-attached temperature sensor
  (PR=E every 3 min) to MQTT, HA and REST, PIC Classic only
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T16:07:00Z'
updated_date: '2026-11-14T22:11:00Z'
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
- [x] #1 Setting PIC temperature sensor (default off, settings.ini PICtempsensor, REST pictempsensor) exists on 2.x; with it on and a PIC Classic in gateway firmware 6.6+ the firmware sends PR=E every 3 min; with it off, or on OT-Direct, or with no PIC, no extra PR=E is sent (host test on the gate, red on current dev)
- [x] #2 A valid PR: E=<value> is published to otgw-pic/temperature_reading and announced to HA as PIC_Temperature_Reading (temperature, C) on the first valid reading; '-' or garbage publishes nothing and announces nothing (ADR-094); the PR=D settings entity is untouched
- [x] #3 The TASK-1314 return water path (D=R, 60 s) keeps working and both share one PR=E reply without a doubled poll (host test)
- [x] #4 build green, evaluate --quick no new FAIL, bench on the Classic (.63) by 09: setting on shows the topic or no entity without a sensor, setting off sends no PR=E
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (95) comparison 1.x (origin/otgw-1.x.x d0a5d290e) vs dev: 36 PIC-related function names in 1.x; all present on dev except pollPicTempSensor, picTempSensorHasData (the TASK-1147 readout) and buildPicDownloadUrl (dev builds the same URL with kPicUpdateUrlFmt, OTGW-Core.ino:6767). The PR= settings readout cycle (15 letters, 3 s), copyPicSettingValue (picSettingValue.h, TASK-1430) and publishAllPICsettings are on dev; dev gates them on isPICEnabled() = state.pic.bAvailable, so OT-Direct reads no PIC settings, as Robert wants. Robert's scope (2026-11-14, in session): only for the PIC Classic; OT-Direct reads no PIC settings.

Port choices: setting in PICBootSection (settings.picBoot.bTempSensor) under the 1.x file key PICtempsensor, so a 1.x settings.ini carries over; REST pictempsensor in GET + knownSettings, updateSetting dispatch (ADR-051 triple). Faux id 239 (1.x used 242, which is OTGWhvacid on dev), Sensors device, flag 0x08 -> otgw-pic/temperature_reading, announced just in time like 241 (queueNonOTDiscoveryIds + skip in the bulk loop). PR=E shares the reply with TASK-1314: pollPicTempSensor sends nothing while picSensorTretPollActive(); the E block keeps feeding Tret for D=R and also publishes the reading when the setting is on. Validity via picParseSensorE (-40..130 C; 1.x used -60..150). Web UI: SET_META + en/nl/de/el label and hint ('PIC Classic only'); the Greek text is mine, unreviewed by a native speaker. Test: tests/test_pic_temp_sensor_port.py (real E block + real gate + source checks), not run yet (compile window).

2026-11-14 18:16 (95) build 2.0.0-beta.254+f12af9f from lane-95-1848 HEAD f12af9f1d on origin/dev ef0c0884b (30d80a30c TASK-1848 beta.252, e4b8362a2 TASK-1849 beta.253, f12af9f1d TASK-1940 beta.254): exit 0, 5 min, 4 Compiling, 1 ESP32S3 image line (warm), RAM 130316 B, Flash 2149603 B; sha256 ino d2c427fbdc521cd9..., littlefs 1e43423a3acc1a6e..., zip f82a7690674d93f2.... Host tests on this HEAD: test_otd_pr_reports_pyotgw 15/15, test_otd_reply_texts_pic 4/4, test_pic_temp_sensor_port 8/8, test_otd_sc_clock 8/8. Old-vs-fix before the rebase: PR test 8 new FAIL on origin/dev OTDirect.ino; reply texts 3 FAIL on the 1848-only code; pic temp 7 FAIL on origin/dev (the setting-off case is trivially green there). evaluate --quick 86/0. Bench requested from 09 (.63 for 1940 + no regression, .61 for 1848/1849).

09 bench: .63 beta.254+f12af9f (b254/): setting off 4.5 min 0x PR=E; on: PR=E queued at 18:40:38 and 18:43:38 (3 min); no reply, no topic, no discovery (.63 has no sensor: expected); settings diff only pictempsensor; web UI row present. .61 beta.255+58b387a (b255/): OT-Direct with the setting on sends 0x PR=E in 4 min; /settings-help.json carries the pictempsensor text. Not seen on hardware: a real sensor reading (no bench unit has a sensor on the PIC); the '-' reply was not visible on .63 because .63 shows no PR: replies at all (separate finding, also on beta.250). 09 note on two texts for one setting -> 205648770 beta.256 drops the inline hint, the i text stays.

95 (2026-11-14 22:4x): regression found and fixed: test/host/test_dhw_water_meter.py no longer compiled since the 1940 commit (the discovery slice of queueNonOTDiscoveryIds() needs picTempSensorHasData() and OTGWpictempid: 'error C3861: picTempSensorHasData: identifier not found', 'COMPILATION FAILED (discovery FIX)'). Fix: OTGWpictempid in FAUX_IDS and a picTempSensorHasData() stub returning false in test_dhw_water_discovery.cpp. After the fix: the full run compiles, 44/45 cases pass (D2 = the #702 unit, see TASK-1949), all 118 mutant checks yes.

95 (2026-11-14 23:0x): second harness broken by the 1940 commit and fixed (test only, no bump): test/host/test_pic_settings_value.py failed 'COMPILATION FAILED (FIX (working tree))' (error C2065 'settings', C3861 'picTempSensorHasData', C2065 'OTGWpictempid', C3861 'setMQTTConfigPending' in the sliced handlePRresponse()). Doubles added in test_pic_settings_value.cpp (setting off); after: RESULT PASS, D1 C1 C2 C3 pass. Sweep of the other slicing harnesses on 47f0ea37e green: 36/36 tests/test_otd_*.py, test_pic_temp_sensor_port, test_settings_whitelist, test_discovery_drip_per_entity, test_ble_discovery_1462, test_sat_energy_discovery, test_sat_health_discovery, test_pic_pm_chain, test/host test_ha_discovery_json, test_cmd_queue_drop, test_override_reply, test_boiler_unsupported_origin.

AC4 bench part ticked by 09 (bench-tester) from its 2026-11-14 bench on .63 (Classic, PIC 6.8 pic16f1847) with beta.254+f12af9f, plus .61 with beta.255/257: setting off (default) 4.5 min telnet 0x PR=E (.63, 18:35:46-18:40:22); setting on at 18:40:37 -> PR=E inserted at 18:40:38.405 and 18:43:38.464 (3 min cadence); no otgw-pic/temperature_reading topic and no PIC_Temperature_Reading discovery in 215 s (.63 has no sensor: 'no entity without a sensor' holds); on OT-Direct (.61, 255) setting on 4 min: 0x PR=E. Settings row 'PIC temperature sensor' under OpenTherm Gateway, i-text only, 0 JS errors (257 smoke, CDP). Build green (95: beta.257 22:39 exit 0) and evaluate are 95's evidence. CAVEAT: on .63 no PIC reply to any PR= is processed at all (TASK-1943), so the 'PR: E=-' reply itself was not seen; 'no entity' is observed, the '-' handling path is covered only by the host test of AC2. Evidence: 09 scratchpad b254/, b255/, b257/.
<!-- SECTION:NOTES:END -->
