---
id: TASK-536
title: Add dump debug info command to debug menu
status: Done
assignee:
  - '@claude'
created_date: '2026-05-04 06:24'
updated_date: '2026-05-04 06:52'
labels:
  - feature
  - debug
  - telnet
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a diagnostic dump command to the debug/telnet interface that serializes the full settings and state objects to the debug stream in a structured, human- and machine-readable format. When a user reports a vague problem, a single dump command gives every relevant value in one paste. Output: streaming key:value pairs via DebugTf(PSTR()), one field per line, grouped by section. Must NOT build a String or large buffer - stream field by field. Entry points: telnet dump command + REST endpoint GET /api/v2/debug/dump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Typing dump on the telnet debug port outputs all settings and state fields, one field per line grouped by section (build, runtime, settings.mqtt, settings.wifi, settings.ntp, state.otgw, state.mqtt, etc.)
- [x] #2 Output is streamed field-by-field using DebugTf(PSTR()) - no String class, no large stack or heap buffer
- [x] #3 Sensitive fields (MQTT password, HTTP password) are masked as *** in the dump output
- [x] #4 Runtime metrics included: free heap bytes, heap fragmentation %, uptime seconds, WiFi RSSI dBm, IP address
- [x] #5 Build metadata included: firmware version string, build number, git hash, compile date/time
- [x] #6 The dump command appears in the telnet help output
- [x] #7 REST endpoint GET /api/v2/debug returns the same info as chunked JSON (not buffered), auth-protected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
handleDebug.ino: added static dumpDebugInfo() that streams 14 sections (build, runtime, settings.*, state.*) via Debugf(PSTR()) field-by-field. Passwords masked as ***. Telnet key 'D' (uppercase, distinct from 'd' for Dallas sim). Help text updated.\n\nrestAPI.ino: added handleDebugDump() handler for GET /api/v2/debug using sendStartJsonMap/sendJsonMapEntry/sendEndJsonMap chunked infrastructure. Auth-protected via checkHttpAuth(). Forward decl + PROGMEM route string kRouteDebugDump + entry in kV2Routes[] before sentinel.\n\nKey constraint: sendJsonMapEntry nameBuf[35] limits key names to 34 chars max. One key shortened: settings.mqtt.disc_verify instead of settings.mqtt.discovery_auto_verify.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added full diagnostic dump to both telnet interface and REST API.\n\nTelnet (handleDebug.ino): pressing 'D' streams 14 sections — [build], [runtime], [settings], [settings.mqtt], [settings.ntp], [settings.sensors], [settings.s0], [settings.outputs], [settings.otgw], [state.otgw], [state.mqtt], [state.pic], [state.debug], [state.uptime] — one key:value line each via Debugf(PSTR()), no String allocation, no heap buffer. Passwords masked as ***.\n\nREST (restAPI.ino): GET /api/v2/debug returns the same fields as chunked JSON via the existing sendStartJsonMap/sendJsonMapEntry/sendEndJsonMap infrastructure. Auth-protected via checkHttpAuth(). Added forward declaration, PROGMEM route string kRouteDebugDump, and entry in kV2Routes[] before sentinel.\n\nConstraint noted: sendJsonMapEntry nameBuf[35] caps key names at 34 chars — one key shortened (settings.mqtt.disc_verify).\n\nBuild: exit 0. Evaluator: 91.7% (pre-existing failures only, none introduced).\n\nFollow-up: TASK-537 created for ESP32/2.0.0 port (SAT fields, ESP32 heap API differences).
<!-- SECTION:FINAL_SUMMARY:END -->
