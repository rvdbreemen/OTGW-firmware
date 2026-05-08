---
id: TASK-592
title: >-
  feat(mqtt): OT-Thing SAT interop - publish nested state JSON and handle
  OT-Thing command topics
status: To Do
assignee: []
created_date: '2026-05-08 16:50'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT Python integration supports an OT-Thing coordinator mode that expects a single MQTT topic '<base>/state' carrying a nested JSON payload with 'slave' and 'thermostat' sub-objects (including slave.status.flame, slave.flow_t, slave.rel_mod, slave.memberId, thermostat.status.ch_enable, etc.). OTGW-firmware 2.0.0 currently publishes per-OT-ID flat topics (the OpenThermGateway-MQTT dialect), which is a different SAT coordinator mode. A user who configures SAT in OT-Thing mode pointed at OTGW-firmware 2.0.0 receives no data because the topic shape does not match.\n\nCommand topics: OT-Thing dialect uses '<base>/chMode1' (payload: 'heat' or 'off'), '<base>/chSetTemp1' (CH setpoint float), '<base>/dwhSetTemp' (DHW setpoint float, note the typo 'dwh' is upstream canon and must be preserved for compat).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New MQTT publish path produces nested slave/thermostat/status JSON on '<settings.mqtt.sTopTopic>/state' when OT-Thing compat mode is enabled (opt-in setting)
- [ ] #2 JSON shape matches otthing.py coordinator contract: slave.status, slave.flow_t, slave.return_t, slave.rel_mod, slave.max_capacity, slave.memberId, slave.ch_set_t, slave.dhw_set_t, thermostat.status.ch_enable, thermostat.status.dhw_enable, thermostat.ch_set_t
- [ ] #3 Subscription handlers added for chMode1, chSetTemp1, dwhSetTemp command topics
- [ ] #4 Existing per-ID flat topic publish unchanged (OpenThermGateway-MQTT dialect continues to work)
- [ ] #5 New setting settings.mqtt.bOtThingCompat (default false) gates the new publish path
- [ ] #6 REST API exposes the compat setting at /api/v2/mqtt/settings
<!-- AC:END -->
