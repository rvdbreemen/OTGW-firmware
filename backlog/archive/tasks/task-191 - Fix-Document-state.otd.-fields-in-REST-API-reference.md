---
id: TASK-191
title: 'Fix: Document state.otd.* fields in REST API reference'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:41'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
  - phase-10
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The OpenAPI spec and REST API documentation do not document the state.otd.* runtime fields exposed via REST. The OTDirectSection struct (OTGW-firmware.h:240) defines 12 fields that are exposed: iScheduleTotal, iScheduleActive, iScheduleDisabled, iOverrideCount, bBypassActive, bStepUpEnabled, bMonitorMode, eMode, bMasterMode, bThermostatConnected, bSetbackActive, iLastThermostatMs. These appear in two REST contexts: (1) /api/v2/device/info as prefixed fields (otdmode, otdbypass, otdmonitor, otdmaster, otdstepup, otdthermostat, otdsetback, otdschedtotal, otdschedactive, otdscheddisabled, otdoverrides, otdirectavailable); (2) /api/v2/otdirect/status as unprefixed fields in an otdirect_status object. Both endpoints need OpenAPI schema entries. Field otdmode is an enum string (gateway/monitor/bypass/master/loopback). All boolean fields reflect read-only runtime state. iLastThermostatMs is not currently exposed via REST.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All state.otd.* fields listed with REST endpoint path, JSON key name, data type, and possible values
- [x] #2 OTDirectStatus schema defined (used by /v2/otdirect/status and mode/settings responses)
- [x] #3 Both the otdirect_status object (dedicated endpoint) and the flattened otd* fields in device/info are documented
- [x] #4 Read-only vs writable access noted for each field
- [x] #5 Documentation consistent with existing REST API reference format
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added OT Direct section to docs/api/README.md documenting all /api/v2/otdirect/* endpoints with full request/response examples, and a comprehensive field reference table cross-mapping all 12 OTDirectSection struct fields to both their REST representations: unprefixed in otdirect_status (dedicated endpoint) and otd-prefixed in device/info (flattened). Notes that iLastThermostatMs is not exposed via REST. Extended device/info example JSON to show all otd* fields. Consistent with existing REST API reference format throughout.
<!-- SECTION:FINAL_SUMMARY:END -->
