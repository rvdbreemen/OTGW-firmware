---
id: TASK-1260
title: >-
  Manual chapters: drop removed OVP MQTT commands (ch05/h05) and sync the SAT
  command table into ch09/h09
status: To Do
assignee: []
created_date: '2026-10-19 17:52'
labels:
  - docs
  - mqtt
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 392000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1256 review. (1) docs/manuals/en/ch05-sat-thermostat.md:58,583,687-690 and nl/h05-sat-thermostaat.md:64,626,736-739 document sat/ovp_enabled, ovp_value, ovp_start, ovp_stop and SATovpvalue/SATovpenabled. OVP calibration was removed in TASK-891.6; src/OTGW-firmware has no ovp symbol and kSatMqttCmds[] (MQTTstuff.ino) has no ovp_* entry, so the firmware drops these as unknown sub-commands. (2) docs/manuals/en/ch09-api-reference.md:1197 and nl/h09-api-referentie.md:988 still carry the old partial SAT command table; docs/MANUAL.md 'SAT Command Topics' was rewritten from kSatMqttCmds[] in TASK-1256 (adds pv_surplus_w, pv_boost_enabled, heating_mode, zone/{1-4}/*, preset values none/activity/home, the passthrough list).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ch05 and h05 no longer list or describe ovp_* MQTT commands or OVP settings; the switch count in the HA entity table matches the remaining list
- [ ] #2 ch09 and h09 SAT command tables match the MANUAL.md table (checked by parsing against kSatMqttCmds[])
<!-- AC:END -->
