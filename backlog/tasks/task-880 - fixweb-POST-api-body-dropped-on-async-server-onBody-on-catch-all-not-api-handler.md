---
id: TASK-880
title: >-
  fix(web): POST /api body dropped on async server (onBody on catch-all, not
  /api handler)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-17 23:19'
updated_date: '2026-06-17 23:31'
labels: []
dependencies: []
ordinal: 96000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Async-migration regression (TASK-865.9). server.on('/api',HTTP_ANY,processAPI) has no onBody; the global server.onRequestBody() is AsyncWebServer::_catchAllHandler->onBody (WebServer.cpp:192) which fires ONLY for catch-all requests. /api/* matches the explicit handler first, so the POST/PUT body is silently dropped -> bodyCompat() empty -> postSettings returns 400 'Invalid JSON'. Confirmed with curl + urllib against a real OTGW32 (alpha.202). Breaks the web UI settings save AND API-based provisioning. Fix: attach webCaptureBody as the /api handler's own onBody (5-arg server.on). Found while building MQTT API-provisioning for the autonomous test workflow + auditing the API per the maintainer's 'does the API still work after the migration' question.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 POST /api/v2/settings with a JSON body returns 200 and the setting is applied (verified on hardware)
- [ ] #2 Build green 3 targets; evaluate.py --quick clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
VERIFIED on hardware (alpha.203). Fix: attach webCaptureBody as the /api handler's own onBody (FSexplorer.ino, 5-arg server.on). POST /api/v2/settings now returns 200 and the value applies. Full end-to-end provisioning confirmed: 5 MQTT settings POSTed (200), /ReBoot (200), device reconnected -> MQTT CONNECTED. New scripts/tests/provision_mqtt.py (active: API upload + reboot + poll mqttconnected) + wired into otgw-test.py as a pre-monitor step. The async web UI settings-save was broken by this same regression -> now fixed for the browser too. bump alpha.203.
<!-- SECTION:NOTES:END -->
