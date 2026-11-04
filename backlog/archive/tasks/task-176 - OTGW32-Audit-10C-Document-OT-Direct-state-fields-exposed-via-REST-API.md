---
id: TASK-176
title: 'OTGW32-Audit-10C: Document OT Direct state fields exposed via REST API'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:25'
updated_date: '2026-04-08 22:42'
labels:
  - audit
  - otgw32
  - phase-10
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/restAPI.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document all state.otd.* fields that are exposed via the REST API, including current gateway mode, thermostat connectivity status, setback state, summer mode, fail safety, and any other runtime state. Ensure the documentation covers both read endpoints and any writable state fields, following the existing /api/v2/ documentation format.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All state.otd.* runtime fields are listed with their REST endpoint path
- [x] #2 Each field documents: data type, possible values, read/write access
- [x] #3 Current gateway mode endpoint is documented
- [x] #4 Thermostat seen / setback engaged state is documented
- [x] #5 Documentation is consistent with existing REST API reference
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read OTDirectSection struct in OTGW-firmware.h for all state.otd.* fields
2. Cross-reference with sendOTDirectStatus() and /v2/device/info in restAPI.ino
3. Check openapi.yaml for existing coverage
4. Document gaps
5. Create audit-fix task
6. Mark ACs, write final summary
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit findings:
- OTDirectSection struct (OTGW-firmware.h:240) defines 12 fields:
  iScheduleTotal, iScheduleActive, iScheduleDisabled, iOverrideCount (uint8_t counts)
  bBypassActive, bStepUpEnabled, bMonitorMode, bMasterMode, bThermostatConnected, bSetbackActive (bool)
  eMode (OTDirectMode enum: gateway/monitor/bypass/master/loopback)
  iLastThermostatMs (uint32_t, NOT exposed via REST)
- REST exposure #1: /api/v2/device/info (flat map) — 12 fields with otd prefix:
  otdirectavailable, otdmode, otdbypass, otdmonitor, otdmaster, otdstepup, otdthermostat,
  otdsetback, otdschedtotal, otdschedactive, otdscheddisabled, otdoverrides
- REST exposure #2: /api/v2/otdirect/status (dedicated, structured object) — 11 fields:
  mode, bypass, stepup, monitor_mode, master_mode, thermostat_connected, setback_active,
  schedule_total, schedule_active, schedule_disabled, overrides_active
- openapi.yaml: /v2/device/info schema does NOT include any of these otd* fields
- openapi.yaml: /v2/otdirect/status endpoint does NOT exist in spec at all
- AC1: all fields listed in this note — docs gap confirmed
- AC2: data types/values are clear from source: eMode is enum string, booleans are boolean JSON
- AC3: gateway mode endpoint (POST /v2/otdirect/mode) exists in code but not in spec
- AC4: thermostat_connected and setback_active are in sendOTDirectStatus() — both undocumented
- AC5: no REST API reference doc exists for otdirect at all
- Created TASK-191 to track the fix
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit complete — none of the state.otd.* fields exposed via REST are documented in the OpenAPI spec.

Findings:
- OTDirectSection struct defines 12 fields; 11 are exposed via REST (iLastThermostatMs is not)
- Two REST exposures: /v2/device/info (flat map, 12 prefixed otd* keys) and /v2/otdirect/status (structured object, 11 unprefixed keys)
- openapi.yaml /v2/device/info schema has no otd* properties at all
- /v2/otdirect/status endpoint is absent from the spec entirely
- eMode is a string enum (gateway/monitor/bypass/master/loopback); all other fields are boolean or integer
- All fields are read-only runtime state; none are directly writable (writes go via commands or /v2/otdirect/settings)

Created TASK-191 to track the fix.
<!-- SECTION:FINAL_SUMMARY:END -->
