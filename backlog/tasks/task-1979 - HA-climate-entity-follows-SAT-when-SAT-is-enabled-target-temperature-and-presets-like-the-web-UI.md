---
id: TASK-1979
title: >-
  HA climate entity follows SAT when SAT is enabled: target temperature and
  presets like the web UI
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T15:53:00Z'
updated_date: '2026-11-15T15:57:00Z'
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
