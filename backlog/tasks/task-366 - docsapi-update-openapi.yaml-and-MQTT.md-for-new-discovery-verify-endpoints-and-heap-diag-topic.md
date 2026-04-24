---
id: TASK-366
title: >-
  docs(api): update openapi.yaml and MQTT.md for new discovery-verify endpoints
  and heap-diag topic
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:47'
updated_date: '2026-04-21 17:05'
labels:
  - code-review
  - docs
  - rest-api
  - mqtt
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3B HIGH: 3 new REST endpoints (/api/v2/discovery GET + POST verify/republish) are absent from openapi.yaml; new MQTT topic otgw-firmware/stats/heap (17-field retained JSON) absent from docs/api/MQTT.md. Documentation-first clients and integrators have no machine-readable contract for the new surfaces.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 openapi.yaml adds paths entries for /v2/discovery, /v2/discovery/verify, /v2/discovery/republish with request/response schemas and 503/409 error cases
- [x] #2 docs/api/README.md gets a short paragraph explaining the relationship between /v2/otgw/discovery (publish all) and /v2/discovery/verify (check retained) and /v2/discovery/republish (mark pending)
- [x] #3 docs/api/MQTT.md gets Heap diagnostic telemetry subsection with full topic path, retention, cadence, 17-field schema, session-counter semantics
- [x] #4 docs/api/MQTT.md gets Retained discovery verification subsection with mechanism, REST endpoints, telnet V key, MQTTdiscoveryAutoVerify setting
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read handler + heap diag template + struct fields
2. Verify line ranges; match actual handler JSON keys
3. Add 3 paths to openapi.yaml
4. Append Discovery verification subsection to docs/api/README.md
5. Add Heap diagnostic telemetry to docs/api/MQTT.md
6. Add Retained discovery verification section to docs/api/MQTT.md
7. Check ACs, add final summary, flip to Done
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified JSON keys in sendMQTTheapdiag against MQTTstuff.ino:1110-1133 (17 fields, exact names).
Verified handler error messages against restAPI.ino:498-520 (503 for MQTT/heap/start-refused, 409 for already-active/drip-in-progress).
Noted task description mentioned last_missing_count/last_orphan_count but actual wire format uses last_missing/last_orphan - documented the wire format.
Added Discovery tag to openapi.yaml tags list.
YAML parses cleanly with yaml.safe_load; all 3 new paths present.
No code changes made.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Documented the three new /api/v2/discovery endpoints and the hourly otgw-firmware/stats/heap MQTT topic introduced in 1.4.1.

Changes:
- docs/api/openapi.yaml: added Discovery tag and three path entries (/v2/discovery GET, /v2/discovery/verify POST, /v2/discovery/republish POST) with full request/response schemas, verbatim 503/409 error messages from restAPI.ino, and examples. Preserved existing /v2/otgw/discovery entry.
- docs/api/README.md: appended "Discovery verification and republish (v1.4.1+)" subsection under the existing Discovery heading. Explains the three endpoints, when to prefer verify over republish, and cross-links ADR-062.
- docs/api/MQTT.md: added "Heap diagnostic telemetry" section with the full retained topic path, hourly ADR-086 (originally ADR-064) cadence and a 17-row table enumerating every JSON field (type, kind, meaning). Added "Retained discovery verification (v1.4.1+)" section covering mechanism, triggers (daily auto-verify, REST, telnet V key), orphan non-deletion rationale, disable instructions and diagnostic interpretation.

Accuracy:
- 17 JSON field names cross-checked against MQTTstuff.ino:1110-1133.
- Response shapes and error messages cross-checked against restAPI.ino:472-527.
- OpenAPI 3.0.3 YAML validated with yaml.safe_load; all three new paths parsed.

No code changes. No ADR or release-note edits (owned by peer agents).
<!-- SECTION:FINAL_SUMMARY:END -->
