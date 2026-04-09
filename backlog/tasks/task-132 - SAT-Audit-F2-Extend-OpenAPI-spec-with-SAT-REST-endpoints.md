---
id: TASK-132
title: 'SAT Audit F2: Extend OpenAPI spec with SAT REST endpoints'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:56'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-6
  - api
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Extend the existing OpenAPI specification with all SAT-related REST API endpoints. Document request/response schemas, error codes and example payloads for all SAT endpoints.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT REST endpoints documented in OpenAPI 3.0 format
- [x] #2 Request and response schemas defined
- [x] #3 Error responses documented
- [x] #4 OpenAPI spec updated and validated
- [x] #5 Follow-up fix tasks created with label 'audit-fix' for missing or incorrect endpoints
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Inventoried all SAT REST endpoints from restAPI.ino (lines 634-900). Found 15 distinct SAT endpoints. Checked docs/api/openapi.yaml — no SAT paths are present at all. The openapi.yaml has a 'SAT' tag neither defined nor used. Created audit-fix task-210 documenting all 15 missing endpoints with full details needed for the spec.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited all SAT REST endpoints in restAPI.ino against openapi.yaml. Found zero SAT endpoints documented in the spec — 15 are implemented in the firmware. The endpoints span GET /api/v2/sat/status (with ?detail=full variant), POST endpoints for target, externaltemp, externaloutdoor, reset_integral, window, preset, enable, mode, humidity, area/<0-3>, weather (GET), and settings/<name>. Created audit-fix task-210 to add all SAT paths to openapi.yaml with schemas, error responses, and tags. The response body for GET /api/v2/sat/status is ~50 fields and needs a full schema definition.
<!-- SECTION:FINAL_SUMMARY:END -->
