---
id: TASK-1332
title: 'SAT: remove the Force PWM option completely (Sergeant D)'
status: To Do
assignee: []
created_date: '2026-10-23T20:03:00Z'
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
- [ ] #1 No firmware path forces PWM any more; the cold cutoff always leaves PWM mode (thermo-nova heating_control.py:148-152/229-234); host test red on HEAD (forced case) and green
- [ ] #2 The setting, REST field, MQTT command, HA entity, MQTT state topic and UI field are gone; a settings file that still holds SATforcepwm loads without error (ADR-192: removed keys ignored), shown by a test or bench run
- [ ] #3 The retained HA discovery config of the force_pwm entity is removed on the device (empty retained publish or the existing retire mechanism), verified on a broker
- [ ] #4 Docs updated (docs/api/MQTT.md, docs/api/openapi.yaml if listed, manuals, firmware-sat-spec)
<!-- AC:END -->
