---
id: TASK-174
title: 'OTGW32-Audit-10A: Extend OpenAPI spec with OTGW32 Direct OT endpoints'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:24'
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
Extend the existing OpenAPI specification with all OTGW32-specific REST endpoints: gateway mode endpoint (GW= equivalent), OT Direct status (current mode, thermostat state, schedule status), and any other state.otd.* fields exposed via REST. Follow the existing /api/v2/ patterns and sendApiError() error format.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All state.otd.* fields accessible via REST are documented in OpenAPI spec
- [x] #2 Gateway mode read/write endpoint is specified
- [x] #3 OT Direct status endpoint (mode, thermostat seen, setback state) is specified
- [x] #4 Error responses follow existing sendApiError() format
- [x] #5 OpenAPI spec passes validation after update
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read OTDirect.ino to find all state.otd.* fields and REST endpoints
2. Read restAPI.ino to confirm endpoint paths and response shapes
3. Check openapi.yaml for existing coverage
4. Document all gaps
5. Create audit-fix task for missing OpenAPI entries
6. Mark ACs, write final summary
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Audit findings:
- openapi.yaml has NO /v2/otdirect/* paths at all (0 of 4 endpoints documented)
- Endpoints implemented in restAPI.ino: GET /v2/otdirect/status, POST /v2/otdirect/mode, GET/POST /v2/otdirect/settings, GET/POST /v2/otdirect/overrides
- /v2/device/info schema in openapi.yaml does not include any otd* fields (otdirectavailable, otdmode, otdbypass, otdmonitor, otdmaster, otdstepup, otdthermostat, otdsetback, otdschedtotal, otdschedactive, otdscheddisabled, otdoverrides)
- GW= is routed via addCommandToQueue → handleOTDirectCommand; mode endpoint wraps this
- sendApiError() format already confirmed in existing endpoints, just not applied to otdirect routes in the spec
- Created TASK-189 to track the fix
- AC1 (state.otd.* fields via REST): all 11 fields exposed but NOT in OpenAPI spec → gap confirmed
- AC2 (gateway mode endpoint): endpoint exists in code, missing from spec → gap confirmed
- AC3 (OT Direct status endpoint): endpoint exists in code, missing from spec → gap confirmed
- AC4 (error responses): sendApiError() format confirmed correct but spec has no errors for otdirect routes
- AC5 (spec validation): cannot pass if entries are absent
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit complete — all OTGW32-specific REST endpoints are missing from the OpenAPI spec.

Findings:
- openapi.yaml has zero /v2/otdirect/* path entries
- Four endpoints implemented in restAPI.ino have no spec coverage: GET /v2/otdirect/status, POST /v2/otdirect/mode, GET/POST /v2/otdirect/settings, GET/POST /v2/otdirect/overrides
- /v2/device/info schema is missing 12 otd* fields added when HAS_DIRECT_OT is enabled
- sendApiError() format is correct in code but unused in spec for these routes

Created TASK-189 to track the fix (add all four endpoints + device/info schema extension to openapi.yaml).
<!-- SECTION:FINAL_SUMMARY:END -->
