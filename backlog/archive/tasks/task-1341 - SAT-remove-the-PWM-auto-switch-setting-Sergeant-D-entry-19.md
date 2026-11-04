---
id: TASK-1341
title: 'SAT: remove the PWM auto switch setting (Sergeant D entry 19)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T09:24:00Z'
updated_date: '2026-10-24T13:50:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 464000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-24 08:47 UTC: 'remove it' (entry 19). Remove settings.sat.bPwmAutoSwitch end to end like Force PWM in TASK-1332: SATtypes.h field, settingStuff read/write (old files with the key load silently), restAPI GET/POST + settings list, MQTT command and HA switch entity if present (retire the retained discovery config), v2.js field, settingsHelp, docs; the sustained-overshoot switch (satCycleCheckAutoSwitch) always runs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No code path reads a PWM auto switch setting; the sustained-overshoot switch always runs; host test red on HEAD (setting off blocks the switch) and green
- [x] #2 Setting/REST/MQTT/HA/UI field gone; an old settings file with the key loads; a retained HA config (if one existed) is cleared
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Remove settings.sat.bPwmAutoSwitch end to end, mirroring TASK-1332 (Force PWM, commit 5a60442ed): SATtypes.h field; settingStuff write + parse (old key SATpwmautoswitch loads silently, ignored); restAPI GET/list/whitelist/POST route; MQTT command + published state sat/pwm_auto_switch_enable; HA discovery switch entity removed and its retained config cleared via clearRetiredSatEntities (PROGMEM table); v2.js field + settingsHelp; docs (MQTT.md, manuals, firmware-sat-spec). satCycleCheckAutoSwitch (SATcycles.ino ~991) no longer checks the setting: all three runtime switches always run (thermo-nova heating_control.py:206/217, entry 19, entry 18). Note to Sergeant D at report time (C13): the setting also blocked the underheat and saturation switches, not only the 60 s one. Host test: old code with the setting off blocks the overshoot switch (red), fix always switches (green); plus source checks that the key is gone from REST/MQTT/UI. The parked TASK-1331 stash touches SATcycles.ino: expect a conflict on pop.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: tests/test_sat_pwm_override.py 70/70 OK on fix; on HEAD SATcycles.ino FAILED 3 (overshoot '1'!='2', underheat '2'!='1', saturation '2'!='1': the old setting off blocked all three switches). test_sat_mqtt_state, test_manual_sat_commands, test_settings_whitelist, test_sat_mqtt_fallback OK; test/host/test_ha_discovery_json.py RESULT PASS incl. 'TASK-1341 FIX: the run emits the empty retained publish to homeassistant/switch/<id>/sat_pwm_auto_switch_enable/config and .../sat/pwm_auto_switch_enable'; node sat-page-order PASS; grep: no bPwmAutoSwitch/pwmautoswitch left in src (only the retired-entity row). evaluate --quick 0 failed. build.bat SUCCESS; bench OTA: boots, settings 177 keys, old SATpwmautoswitch key still in /settings.ini and loads without error. Retained-clear not observable on the bench (temp broker had no retained config); covered by the host discovery harness.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT setting 'PWM auto switch' is removed end to end (setting, settings file, REST, MQTT command and state topic, HA switch, v2 UI, help, docs), as Sergeant D decided (entry 19). satCycleCheckAutoSwitch always runs the overshoot, underheat and saturation switches (thermo-nova heating_control.py:206/217). The retired HA switch config and state topic are cleared through kRetiredSatEntities; an old stored key loads and is ignored.
Evidence: host test 70/70 on fix, 3 failures on HEAD; HA discovery harness PASS with the retained clear; bench boot with the old key. Not verified on hardware: the retained clear on a broker that held the old config.
<!-- SECTION:FINAL_SUMMARY:END -->
