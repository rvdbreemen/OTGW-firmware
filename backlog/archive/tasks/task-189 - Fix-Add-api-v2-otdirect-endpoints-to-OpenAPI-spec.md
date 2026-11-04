---
id: TASK-189
title: 'Fix: Add /api/v2/otdirect endpoints to OpenAPI spec'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:40'
updated_date: '2026-04-08 22:52'
labels:
  - audit-fix
  - otgw32
  - phase-10
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The OpenAPI spec (docs/api/openapi.yaml) has no coverage of the /api/v2/otdirect/* endpoints implemented in restAPI.ino. The following paths and the state.otd.* fields they return need to be added: GET/POST /v2/otdirect/status, POST /v2/otdirect/mode, GET/POST /v2/otdirect/settings, GET/POST /v2/otdirect/overrides. The /v2/device/info response schema also needs the otdirectavailable, otdmode, otdbypass, otdmonitor, otdmaster, otdstepup, otdthermostat, otdsetback, otdschedtotal, otdschedactive, otdscheddisabled, otdoverrides fields added.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add /v2/otdirect/status GET endpoint with full OTDirectStatus schema
- [x] #2 Add /v2/otdirect/mode POST endpoint with mode parameter enum
- [x] #3 Add /v2/otdirect/settings GET/POST endpoint
- [x] #4 Add /v2/otdirect/overrides GET/POST endpoint with action parameter
- [x] #5 Extend /v2/device/info schema with all otd* fields
- [x] #6 OpenAPI spec passes validation after update
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 4 new /api/v2/otdirect/* path entries to openapi.yaml (status GET, mode POST/PUT, settings GET/POST/PUT, overrides GET/POST/PUT), added new OT Direct (OTGW32) tag, added OTDirectStatus reusable schema, and extended /v2/device/info schema with all 12 otd* fields (marked optional, OTGW32 only). YAML validated clean with Python yaml.safe_load.
<!-- SECTION:FINAL_SUMMARY:END -->
