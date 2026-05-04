---
id: TASK-534
title: >-
  Fix: DHW setpoint shows HA initial value (43°C) and DHW temperature unknown in
  HA via MQTT
status: To Do
assignee: []
created_date: '2026-05-04 06:11'
updated_date: '2026-05-04 06:11'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/543'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
stefan reports in #nederlandse-ondersteuning that the DHW setpoint shows 43°C in HA (the HA climate entity initial placeholder value) while the firmware UI correctly shows 60°C. The DHW temperature sensor (sensor.opentherm_gateway_otgw_otgw_dhw_temperature) shows 'unknown'. Firmware log confirms TdhwSet = 60°C is received correctly. Still present in beta.6 (confirmed 2026-05-03). Pattern is identical to GitHub #543 (Max CH water setpoint 0°C in HA Boiler entity). Likely cause: MQTT topic the firmware publishes to does not match the state_topic in the HA discovery config, OR stale retained discovery config from an older firmware version is overriding the current one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 DHW setpoint in HA reflects the actual boiler value (60°C), not the 43°C placeholder
- [ ] #2 DHW temperature sensor in HA shows a value or correctly reports unavailable when the boiler does not send OT ID 26
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-03: stefan sent screenshot of firmware UI showing TdhwSet=60°C in the OT monitor log, dashboard shows Domestic Hot Water Setpoint = 60°C correctly. In HA (via MQTT), the DHW setpoint entity shows 43°C (the HA discovery initial placeholder) and the DHW temperature entity shows 'unknown'. Firmware is on v1.4.1 originally, confirmed still present in beta.6. stefan confirmed he only uses MQTT integration (not the HA OTGW core component simultaneously). Waiting for: MQTT Explorer dump showing which topics are retained under homeassistant/climate/otgw-*/dhw*/config and what state_topic they reference; and which MQTT topic the firmware publishes TdhwSet to.
<!-- SECTION:NOTES:END -->
