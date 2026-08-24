---
id: TASK-1081
title: Add device_class water to HA DHW flowrate sensor
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-24 18:12'
updated_date: '2026-08-24 20:20'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-24: implemented as volume_flow_rate, NOT water as the issue literally asked. Verified against Home Assistant core: device_class water accepts only cumulative volume units (WATER_USAGE_UNITS: L, gal, m3, ft3, CCF, MCF) per homeassistant/components/energy/validate.py, and the developer sensor docs list l/min only under volume_flow_rate. Setting water on this l/min measurement sensor would make HA reject the entity.

AC #2 (Energy dashboard) is NOT met by this change and cannot be: the Energy dashboard needs a cumulative volume, which is a separate derived entity covered by ADR-090 (1.x) and ADR-176 (2.0.0), both still Proposed.
<!-- SECTION:NOTES:END -->
