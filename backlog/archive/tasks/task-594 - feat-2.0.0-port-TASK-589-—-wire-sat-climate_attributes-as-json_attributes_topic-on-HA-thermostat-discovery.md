---
id: TASK-594
title: >-
  feat-2.0.0: port TASK-589 — wire sat/climate_attributes as
  json_attributes_topic on HA thermostat discovery
status: Done
assignee:
  - '@claude'
created_date: '2026-05-08 21:55'
updated_date: '2026-05-08 22:52'
labels:
  - mqtt
  - ha-discovery
  - sat
  - port-from-dev
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-589. MQTTHaDiscovery.cpp:streamClimateDiscovery does not include json_attributes_topic for climateIdx==0. The sat/climate_attributes publish (SATcontrol.ino:2534) sends SAT PID/curve attributes but the HA thermostat entity does not know to consume them. Fix: add json_attributes_topic field pointing to <mqttPubTopic>/sat/climate_attributes in streamClimateDiscovery for climateIdx==0, using the existing kJsonAttrTopic PROGMEM constant (line 2066). kJsonAttrTopic write pattern already used at line 2230.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 json_attributes_topic added to streamClimateDiscovery in MQTTHaDiscovery.cpp for climateIdx==0 only (thermostat); DHW control unchanged
- [x] #2 Build exits 0 for both ESP8266 and ESP32 targets
- [x] #3 evaluate.py --quick shows no new failures
- [x] #4 Prerelease bump committed alongside the firmware change
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported TASK-589 from dev: wired sat/climate_attributes as json_attributes_topic in HA MQTT discovery for the SAT climate entity.

Changes:
- MQTTHaDiscovery.cpp: added json_attributes_topic field in the climate discovery payload for climateIdx==0, pointing to sat/climate_attributes
- Enables HA to display SAT operational attributes (modulation, pressure, flame state, etc.) as entity attributes in the climate card

Tests:
- python build.py (ESP8266 + ESP32-S3 both clean, alpha.34)
- python evaluate.py --quick: 95.6% health, no new violations
- AC 1-4 all verified
<!-- SECTION:FINAL_SUMMARY:END -->
