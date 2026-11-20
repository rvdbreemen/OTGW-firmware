---
id: TASK-2971
title: >-
  HA discovery table: tidy the device page, binary device classes, BLE
  availability, leftovers (HA review H1 H3 H5 H7)
status: To Do
assignee:
  - '@otgw-dev'
created_date: '2026-11-20T15:53:00Z'
labels:
  - mqtt
  - ha
  - ux
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1103000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2970 HA review (docs/reviews/2026-11-20_ux-ha-entities/README.md, H1, H3, H5, H7); umbrella TASK-2970. H1: of 405 sensor rows in mqttHaSensors[] (MQTTHaDiscovery.cpp:871-1436), 221 are uncategorised and enabled by default, so they fill HA's main Sensors list; 38 of them are raw byte halves (_hb/_lb/_u8, e.g. MQTTHaDiscovery.cpp:563, :566). H3: OpenTherm binary rows have no device class (MQTTHaDiscovery.cpp:1450+), so HA shows On/Off. H5: BLE discovery (MQTTstuff.ino:3733+) has no availability and no expire_after. H7: typo VH_free_ventliation_status (:815), Cooling_configs (:792), placeholder %sensor_id% (:709), override names outside the naming rule (:3339-3341). Changing the discovery payload is part of the HA contract: otgw-architect first checks whether this falls under an existing ADR (ADR-077 streaming discovery, ADR-101 flat topics, ADR-140 source prefixes) or needs a Proposed one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otgw-architect has recorded in this task whether the discovery changes fall under an existing ADR or need a Proposed ADR, before the table is edited
- [ ] #2 H1: no _hb, _lb or _u8 row and no status-byte row is uncategorised and enabled by default (entity_category diagnostic, enabled_by_default false); SAT tuning internals (PID terms, Kp/Ki/Kd, averages) are diagnostic; the before/after count of uncategorised and enabled sensor rows is recorded; a test over the table enforces the rule for new rows
- [ ] #3 H3: fault rows carry device_class problem, Boiler/Thermostat Connected carry connectivity, flame/pump/active rows carry running; the golden discovery test covers three examples
- [ ] #4 H5: BLE entities carry the gateway availability topic and an expire_after of about 3 report intervals; a host test checks the payload
- [ ] #5 H7: the ventilation typo and the Cooling_configs plural are fixed, the %sensor_id% placeholder row is removed or documented, and override names follow the naming rule
- [ ] #6 Topics and uniq_id are unchanged (golden topic test green), so existing HA installs keep their entity ids
<!-- AC:END -->
