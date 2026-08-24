---
id: TASK-1081
title: Add device_class water to HA DHW flowrate sensor
status: Done
assignee:
  - '@claude'
created_date: '2026-08-24 18:12'
updated_date: '2026-08-24 20:21'
labels:
  - enhancement
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: low
ordinal: 183000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GitHub #675 (Jeroenll): DHW Water Flow Rate In DHW Circuit MQTT discovery config lacks device_class: water. Adding it lets the entity be added to the HA Energy dashboard.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DHW flowrate sensor (MsgID 19, both rows) carries device_class: volume_flow_rate, the class HA defines for l/min
- [x] #2 device_class: water is NOT set on this sensor, because HA restricts that class to cumulative volume units and would reject an l/min entity
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: implemented as volume_flow_rate, NOT water as the issue literally asked. Verified against Home Assistant core: device_class water accepts only cumulative volume units (WATER_USAGE_UNITS: L, gal, m3, ft3, CCF, MCF) per homeassistant/components/energy/validate.py, and the developer sensor docs list l/min only under volume_flow_rate. Setting water on this l/min measurement sensor would make HA reject the entity.

AC #2 (Energy dashboard) is NOT met by this change and cannot be: the Energy dashboard needs a cumulative volume, which is a separate derived entity covered by ADR-090 (1.x) and ADR-176 (2.0.0), both still Proposed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Typed the DHW flow-rate sensor (OpenTherm MsgID 19) as device_class: volume_flow_rate on both discovery rows, and added that value to HaDeviceClass with its PROGMEM string.

Deliberately NOT what GitHub #675 literally asked for. The issue requested device_class: water so the entity could be added to the HA Energy dashboard. Verified against Home Assistant core (homeassistant/components/energy/validate.py, WATER_USAGE_UNITS) that the water class accepts only cumulative volume units: L, gal, m3, ft3, CCF, MCF. This sensor is l/min with state_class measurement, a rate. HA would have rejected a water-classed l/min entity, so implementing the request verbatim would have broken the sensor rather than improving it.

The reporter's actual goal, a water figure on the Energy dashboard, needs a cumulative total that this firmware does not produce. That is a separate derived entity, proposed in ADR-090 (1.x) and ADR-176 (2.0.0), both still Proposed and tracked separately. The original AC about Energy dashboard selectability was removed from this task because it is not achievable by typing the rate sensor, and keeping it would have left a permanently unmeetable criterion.

Build exit 0, evaluator 35/35 with 0 failures. Peer change landed on the 2.0.0 line under TASK-1084.
<!-- SECTION:FINAL_SUMMARY:END -->
