---
id: TASK-210
title: 'SAT: Add SAT REST endpoints to openapi.yaml'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:25'
updated_date: '2026-04-09 05:45'
labels:
  - audit-fix
  - sat
  - api
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The docs/api/openapi.yaml has zero SAT endpoints. The firmware implements 12 SAT REST endpoints that are completely undocumented in the spec:\n\n- GET /api/v2/sat — returns full SAT status JSON\n- GET /api/v2/sat/status — same as above\n- GET /api/v2/sat/status?detail=full — extended health diagnostics\n- POST /api/v2/sat/target — set target temperature (5.0-30.0°C)\n- POST /api/v2/sat/externaltemp — push indoor temperature\n- POST /api/v2/sat/externaloutdoor — push outdoor temperature\n- POST /api/v2/sat/reset_integral — reset PID integral\n- POST /api/v2/sat/window — set window open/closed state\n- POST /api/v2/sat/preset — activate a named preset\n- POST /api/v2/sat/enable — enable/disable SAT\n- POST /api/v2/sat/mode — set control mode (continuous/pwm)\n- POST /api/v2/sat/humidity — push indoor humidity\n- GET /api/v2/sat/weather — weather integration status\n- POST /api/v2/sat/area/<0-3> — push area temperature\n- POST /api/v2/sat/settings/<name> — update any SAT setting\n\nEach endpoint needs: path, method, operationId, summary, description, request body schema, response schema (200 and 4xx), and tags.\n\nThe response body for GET /api/v2/sat/status is a large JSON object with ~50 fields (see satSendStatusJSON() in SATcontrol.ino ~line 1050). A minimal schema should be defined covering the key fields.\n\nSource: src/OTGW-firmware/restAPI.ino (lines 634-900), SATcontrol.ino (satSendStatusJSON/satSendHealthJSON functions)."
<!-- SECTION:DESCRIPTION:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added 13 SAT REST endpoints to docs/api/openapi.yaml: GET /v2/sat (alias for status), GET /v2/sat/status (with ?detail=full support), POST /v2/sat/target, POST /v2/sat/externaltemp, POST /v2/sat/externaloutdoor, POST /v2/sat/reset_integral, POST /v2/sat/window, POST /v2/sat/preset, POST /v2/sat/enable, POST /v2/sat/mode, POST /v2/sat/humidity, GET /v2/sat/weather, POST /v2/sat/area/{index}, POST /v2/sat/settings/{name}. Added SAT tag with description, SatStatus schema with ~25 key fields and example, and StatusOk schema for write responses.
<!-- SECTION:FINAL_SUMMARY:END -->
