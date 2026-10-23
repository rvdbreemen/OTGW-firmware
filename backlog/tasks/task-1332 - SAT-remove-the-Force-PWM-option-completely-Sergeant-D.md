---
id: TASK-1332
title: 'SAT: remove the Force PWM option completely (Sergeant D)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T20:03:00Z'
updated_date: '2026-10-23T20:58:00Z'
labels:
  - sat
  - mqtt
  - webui
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 455000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-23 19:44-19:45Z (docs/sat/sergeant-d-corrections.md entry 13): 'Honestly, I don't even know why we still have it in the code' / 'It is better to be removed completely'. thermo-nova only stores force_pulse_width_modulation and never reads it (const.py:80/149, config_flow.py:728, entry_data.py:58/223). Firmware uses: settings.sat.bForcePWM (SATtypes.h), the per-tick force in satControlLoopRun, the Force PWM branch in satPwmDisableOnHeaterOff (TASK-1327), settingStuff.ino read/write (SATforcepwm), restAPI.ino GET (satforcepwm) + POST (force_pwm) + settings list, MQTTstuff.ino command table (force_pwm), MQTTHaDiscovery.cpp entity (sat/force_pwm, force_pwm_enable), SATcontrol.ino MQTT publish (sat/force_pwm_enable) and JSON field (force_pwm), v2.js SET_META + SAT_L3 'PID / PWM' group, settingsHelp.h text, docs (MQTT.md, openapi, manuals, firmware-sat-spec).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No firmware path forces PWM any more; the cold cutoff always leaves PWM mode (thermo-nova heating_control.py:148-152/229-234); host test red on HEAD (forced case) and green
- [x] #2 The setting, REST field, MQTT command, HA entity, MQTT state topic and UI field are gone; a settings file that still holds SATforcepwm loads without error (ADR-192: removed keys ignored), shown by a test or bench run
- [x] #3 The retained HA discovery config of the force_pwm entity is removed on the device (empty retained publish or the existing retire mechanism), verified on a broker
- [x] #4 Docs updated (docs/api/MQTT.md, docs/api/openapi.yaml if listed, manuals, firmware-sat-spec)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Relies on Sergeant D entries 13 (remove Force PWM), 8/10 (SAT picks the mode itself), 12 (cutoff as thermo-nova: leave PWM). Remove bForcePWM end to end: SATtypes.h field; settingStuff.ino write/read of SATforcepwm (reading an old file with the key must stay silent/harmless, ADR-192 'ignore removed keys'); restAPI.ino GET addBool + POST 'force_pwm' branch + settings name list; MQTTstuff.ino command table row; SATcontrol.ino per-tick force, the bForcePWM branch in satPwmDisableOnHeaterOff (now always leaves PWM mode on the heater-off branch), JSON field force_pwm, MQTT publish sat/force_pwm_enable + its shadow; MQTTHaDiscovery.cpp entity row; v2.js SET_META satforcepwm + SAT_L3 group entry; settingsHelp.h text; docs (docs/api/MQTT.md, openapi if listed, manuals, firmware-sat-spec, the TASK-1322 sub text in v2 'forcing PWM is a setting under SAT tuning' must change). HA cleanup: publish a zero-length retained payload once per MQTT (re)connect/discovery pass to the old force_pwm entity discovery topic(s) (reuse the publishEmptyDiscoveryFor/mqttPublishRaw pattern; small table of retired entities) and stop publishing sat/force_pwm_enable. Tests: test_sat_pwm_override.py forced-PWM cases rewritten: the cutoff always leaves PWM; source checks that bForcePWM/satforcepwm/force_pwm are gone from src; a settings-load host test or bench run with an old settings file containing SATforcepwm; webui test that the field is gone. Bench: flash, check REST settings has no satforcepwm, broker shows the retired discovery topic emptied. Cross-line: no (SAT is 2.0.0 only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 23:00 evidence. AC1: test_sat_pwm_override.py fix 'Ran 63 OK', HEAD copies 'FAILED (failures=3)' (cutoff_leaves_pwm_mode, stale held CS, closed valves; HEAD ran the forced path); source check: no bForcePWM/satforcepwm/SATforcepwm/force_pwm in src except the RETIRED-SAT-ENTITY row. AC2: bench (OTGW32, app-only flash): uploaded settings.ini with "SATforcepwm": true, rebooted (uptime 0), no crashlog; REST settings identical to the pre-flash snapshot on all 177 other keys, satforcepwm gone. AC3: bench broker (temp mosquitto 192.168.88.32): old alpha.476 left homeassistant/switch/otgw-1020BA21B4F8/sat_force_pwm_enable/config retained; fix: during the ID-0 discovery pass (OT traffic via sat_boiler_emulator on port 25238, enabled temporarily and restored to off) the topic received an empty retained publish '(null)' and is gone. Final PROGMEM build: a hand-published retained probe on that topic was cleared after reboot. Host test test_ha_discovery_json.py asserts the empty publish (PASS; FAIL with the call commented out). Switch uniq_ids/topics of the 8 switches present before and after are identical (renumbering only changes case indices, ids come from the name string). AC4: docs updated (MQTT.md, openapi.yaml, api README, manuals en/nl ch04/05/09, MANUAL.md, c4-code-sat, features, sat-comparison, firmware-sat-spec, ADR-195 exception resolved by entry 13). Review (opus, incl. Sergeant D lens): no correctness defect; fixed PROGMEM table warning, too-broad host-test exemption, CHANGELOG lazy continuation, stale switch counts (15), spec line, goldens/gen_sat_props. Gates: evaluate 0 fail 0 warn; build.bat combo SUCCESS 22:47; test_static_integrity 33 OK; sat-page-order 0 FAIL; test_sat_mqtt_state OK; adr governance OK.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed SAT's Force PWM option completely (Sergeant D entry 13): setting, REST field, MQTT command, HA switch, MQTT state topic and UI field; the cold cutoff and closed valves always leave PWM mode. Old settings files with SATforcepwm load unchanged; the retained HA discovery config and state topic of the removed switch are cleared on each ID-0 discovery pass. Evidence: AC1 host tests red on HEAD (3) / green (63); AC2 bench reboot with an old-key settings file, 177 settings intact, no crash; AC3 bench broker: retained config present on alpha.476, emptied by the fix (and a re-published probe cleared by the final build); AC4 docs and ADR-195 updated. Not verified: a real Home Assistant instance (broker-level only).
<!-- SECTION:FINAL_SUMMARY:END -->
