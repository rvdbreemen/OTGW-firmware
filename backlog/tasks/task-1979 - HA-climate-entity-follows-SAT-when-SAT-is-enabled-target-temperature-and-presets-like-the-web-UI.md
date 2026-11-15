---
id: TASK-1979
title: >-
  HA climate entity follows SAT when SAT is enabled: target temperature and
  presets like the web UI
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T15:53:00Z'
updated_date: '2026-11-15T16:07:00Z'
labels:
  - feature
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request by indigo_light (#beta-testing, https://discord.com/channels/812969634638725140/914498730001072149/1571549406790946897, beta.250): the firmware SAT has no usable climate entity in HA; the standard thermostat card does not work and the target can only be set through the preset temperatures in the web UI. Robert (maintainer) approved and asked the bot to research the best way, https://discord.com/channels/812969634638725140/914498730001072149/1571549972367810602: 'de huidige climate-entity is nog pre-SAT. Kan je het zo maken dat als SAT ingeschakeld dat de climate-entity volledig de features bevat van de webUI variant.' Current state (MQTTHaDiscovery.cpp:3023-3197): climate.Thermostat exists with curr_temp /Tr, temp_stat /TrSet, temp_cmd TT={{ value }} (:3148), modes off/heat/cool (:3100), no preset_modes, SAT data only as json_attributes /sat/climate_attributes (:3184). With SAT enabled the gateway refuses TT= (ADR-179), so the card's target temperature does nothing then.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Research: options compared (adapt the existing climate entity while SAT is on vs a separate SAT climate entity), incl. what each means for current HA users (entity id, automations), HA MQTT climate discovery fields (preset_modes, preset_mode_command/state, temperature command to the SAT target), ADR-077/101/140, and SAT preset behaviour cited from thermo-nova (file:line at 7beaaf5) checked against docs/sat/sergeant-d-corrections.md; recommendation decided by Robert
- [ ] #2 With SAT enabled the HA card's target temperature and preset change SAT's target and preset as the web UI does, and the card shows SAT's state
- [ ] #3 With SAT disabled the entity behaves as today (TT=)
- [ ] #4 Host test on the discovery payload red on the old code, green on the fix; bench on both board types with an HA screenshot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 research (92, 2026-11-15, no code). Today: climate.Thermostat (MQTTHaDiscovery.cpp:3023-3197) curr /Tr, target /TrSet, cmd TT= (:3148), modes off/heat/cool without a mode command, no presets, SAT only as json_attributes (:3184). With SAT on TT= is refused (ADR-179), so the card does nothing. SAT already has set/<node>/sat/target (MQTTstuff.ino:675), set/<node>/sat/preset (:686, also an HA select, MQTTHaDiscovery.cpp:3567), state sat/target (SATcontrol.ino:5106), sat/preset (:4988), sat/room_temp (:5199, SAT's own room input, may be an external/BLE sensor). A SAT toggle (satHandleEnabled SATcontrol.ino:3736-3744 -> updateSetting SATenabled settingStuff.ino:1096-1115) republishes no discovery.

thermo-nova 7beaaf5 (custom_components/sat, opened): features TARGET_TEMPERATURE|PRESET_MODE (climate.py:858-865); presets activity/away/home/sleep/comfort from config (:870-871), preset_modes [none]+those (:101); set_temperature equal to a preset selects it (:557-568), firmware satHandleTargetTemp SATcontrol.ino:3688-3710 mirrors it; preset none restores the pre-custom target (:604-624), firmware SATcontrol.ino:2313-2326. Firmware has an extra eco preset (existing deviation, not this task). satHandlePreset (SATcontrol.ino:2284) has no SAT-enabled check. Sergeant D entries: 223, 308 no conflict; 311 ('the SAT climate entity already handles the room setpoint') supports one climate entity that sets SAT's target; 326 (hot/cold tolerance) stays open, HA mqtt/climate.py has no tolerance field.

HA (core-dev mqtt/climate.py): preset_modes must not include none (:213-216), preset_modes and preset_mode_command_topic are vol.Inclusive (:300-306). Options: A1 one entity, discovery content changes with SAT and is republished on each toggle (new republish path, features change under the user). A2 one entity, fixed discovery, firmware routes by SAT state: new command topic (SAT on -> SAT target, off -> TT= as today), state topics for target/current temp/preset that carry SAT's values while on and /TrSet, /Tr, none while off, re-published retained on every SAT toggle; fixed preset_modes comfort,eco,away,sleep,activity,home. B a separate SAT climate entity (new uniq_id): old entity untouched but dead while SAT is on, SAT users re-point dashboards/automations, two cards. Recommendation A2: same entity id for current users, SAT off identical to today, no discovery republish logic; cost: new topics re-published on toggle, presets visible while SAT is off. Out of scope: switching SAT on/off from the card mode (stays the SAT switch). Contract change: Proposed ADR via adr-kit after Robert's choice, docs/api/MQTT.md + manual HA section.

Design input in #beta-testing (2026-11-15): Sergeant D 15:53 https://discord.com/channels/812969634638725140/914498730001072149/1571553680577138709 proposes the existing mqtt climate entity with two cases (1: physical thermostat attached, in sync, nothing to do; 2: standalone with SAT enabled, SAT displayed as OTGW_Thermostat), 'This is an open discussion'. Robert 15:54 (1571553998408913006): 'I think standalone still could mean control the settings from home assistant.' Sergeant D 16:01 (1571555811304407263) points indigo_light to the follow-thermostat option (SATfollowthermostat, entries 2 and 169). Recommendation A is compatible (existing entity); Sergeant D's case split is part of A; proposal entry text sent to the coordinator for detection.
<!-- SECTION:NOTES:END -->
