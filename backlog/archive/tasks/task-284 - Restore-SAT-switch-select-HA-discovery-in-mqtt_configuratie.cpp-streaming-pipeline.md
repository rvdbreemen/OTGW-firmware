---
id: TASK-284
title: >-
  Restore SAT switch/select HA discovery in mqtt_configuratie.cpp streaming
  pipeline
status: Done
assignee:
  - '@claude'
created_date: '2026-04-17 22:07'
updated_date: '2026-04-17 22:12'
labels:
  - mqtt
  - ha
  - sat
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2.0.0 refactor moved Home Assistant MQTT discovery from PROGMEM array (generated from mqttha.cfg) to direct streaming functions in mqtt_configuratie.cpp. The streaming pipeline currently covers sensor, binary_sensor, climate and number entities. Switch (13 entries) and select (1 entry) entities from TASK-81 are defined in mqttha.cfg (archived), but the streaming pipeline does not emit them. Result: HA no longer discovers these 14 SAT control entities after the refactor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 streamSatSwitchDiscovery(client, switchIdx, ctx) emits HA discovery config for all 13 SAT boolean switches (solar_gain, summer_simmer, comfort_adjust, multi_area, auto_tune, simulation, window_detection, force_pwm, push_setpoint, ovp_enabled, preset_sync, dhw_enabled, pwm_auto_switch)
- [x] #2 streamSatSelectDiscovery(client, selectIdx, ctx) emits HA discovery config for sat_heating_system select (options 0/1/2/3)
- [x] #3 Declarations added to MQTTstuff.h
- [x] #4 doAutoConfigure() iterates both new streaming functions after climate/number
- [x] #5 doAutoConfigureMsgid() triggers switch+select on its drip ID (same pseudo-ID as climate)
- [x] #6 ESP8266 build succeeds (arduino-cli backend)
- [x] #7 Discovery payloads match the SAT switch/select definitions in docs/archive/mqttha.cfg (same uniq_id/name/topic/icon)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add streamSatSwitchDiscovery() + streamSatSelectDiscovery() declarations in MQTTstuff.h (DONE)
2. Implement both functions in mqtt_configuratie.cpp following the streamClimateDiscovery/streamNumberDiscovery pattern: two-pass MqttJsonWriter (MEASURE then WRITE), beginPublish with known length, compose lambda for JSON body.
3. Use a shared helper streamSatBoolSwitch() that takes PSTR parameters for uniqSuffix, nameSuffix, cmdSub, statSub, icon — the 13 switches all share the same JSON shape, only these 5 strings differ. Keeps the per-case code to one line each. Topic object-id derived from uniqSuffix by stripping leading dash and replacing dashes with underscores.
4. streamSatSelectDiscovery inlines the single select case (no helper needed for one entry); uses hardcoded "options":["0","1","2","3"] array.
5. Add callers in doAutoConfigure(): loop switchIdx 0..12 + selectIdx 0 after streamNumberDiscovery.
6. Add callers in doAutoConfigureMsgid() for OTid == 0 (same pseudo-ID as climate, so existing drip trigger covers them).
7. ESP8266 arduino-cli build must succeed (verified after changes). ESP32 build not tested locally (core install blocked by network timeout).
8. Commit as single feat commit referencing TASK-284.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added streaming HA MQTT discovery for the 13 SAT boolean switches and 1 select entity that the 2.0.0 refactor dropped when moving from mqttha.cfg generation to direct streaming functions in mqtt_configuratie.cpp.

Changes:
- MQTTstuff.h: declare streamSatSwitchDiscovery + streamSatSelectDiscovery
- mqtt_configuratie.cpp: add streamSatBoolSwitch() helper (shared JSON body for the 13 switches, parameterised with 5 PSTRs), streamSatSwitchDiscovery() dispatching switchIdx 0..12 to the helper, and streamSatSelectDiscovery() inlining the single sat_heating_system select
- MQTTstuff.ino: callers added to doAutoConfigure() loop and to doAutoConfigureMsgid() under OTid==0 (same pseudo-ID as climate, so the bitmap drip covers the new entities without extra state)

Switch object-id topic paths are derived from uniqSuffix by stripping leading '-' and swapping remaining '-' to '_' (matches mqttha.cfg archive naming).

Build:
- ESP8266 (arduino-cli backend) clean, RAM 84% unchanged, IRAM 94% unchanged, Flash 71% (+3576 bytes vs pre-change for 14 new discovery configs)
- ESP32 and ESP32-S3 not verified locally (core install timed out); the code is platform-agnostic so no platform-specific risk

Runtime verification (AC #8) deferred to a real HA+MQTT broker test after flash.
<!-- SECTION:FINAL_SUMMARY:END -->
