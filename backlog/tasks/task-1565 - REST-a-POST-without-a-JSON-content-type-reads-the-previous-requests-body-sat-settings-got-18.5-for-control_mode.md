---
id: TASK-1565
title: >-
  REST: a POST without a JSON content type reads the previous request's body
  (sat/settings got 18.5 for control_mode)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T18:59:00Z'
updated_date: '2026-11-03T19:23:00Z'
labels:
  - rest-api
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 716000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on bench .63, 2.0.0-beta.14+4d94f13, 2026-11-02 19:54 (TASK-1542 bench). Request 1: POST /api/v2/settings, Content-Type application/json, body {"name":"sattargettemp","value":18.5} -> ok. Request 2, right after: curl -X POST -d "pwm" /api/v2/sat/settings/control_mode (curl default Content-Type application/x-www-form-urlencoded) -> response {"status":"ok","setting":"control_mode","value":"18.5"}. The handler (restAPI.ino sat/settings branch, hasArgCompat(F("plain")) / argCompat(F("plain")) -> satExtractPostValue) got the value of the previous request's body. With JSON content type the same call returned value "pwm". Risk: a setting written with another request's value (here harmless: atoi("18.5")=18 matches no control mode). Needs a reproduction (host or bench: two requests in a row, the second form-encoded), the root cause in the async body/arg capture (stale plain buffer not cleared per request), and an old-vs-fix test.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduction on the bench or a host harness: a form-encoded POST right after a JSON POST gets the previous body on the old code
- [x] #2 Fix: each request sees only its own body; a missing body gives 400 Missing value; host test red on the old code, green on the fix
- [ ] #3 Bench re-run of the same two curl calls on the fix returns value pwm or 400, never the previous value
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-03 fix (sonnet agent; lead checked the vendored ESPAsyncWebServer: one request per connection, _onDisconnectfn runs in AsyncWebServerRequest::_onDisconnect() just before delete in AsyncWebServer::_handleDisconnect, WebServer.cpp:131; form-urlencoded and key=value text/plain bodies are parsed as params and never reach the body hook, WebRequest.cpp:202-216). Root cause webServerCompat.h webCaptureBody/hasBodyCompat: g_requestBody never released, next request at the freed address passed the owner match. Fix: release on the request's onDisconnect (owner-guarded). test/host/test_rest_body_stale.py --old-rev HEAD: OLD S2 FAIL, FIX all pass, RESULT PASS (lead rerun in the integration tree). Contract unchanged: a form-encoded call now gets 400 Missing value; JSON as documented. Residual: a request destroyed while its client is still connected (destructor nulls the callback) is not covered. Tag beta.58; AC3 bench curl pending in the shared build.

2026-11-03 BENCH beta.59+c2983e1 on both units: first fix does NOT work on the device. JSON POST heating_mode 'comfort' then form-encoded 'zzz' returned value 'comfort' (the echoed input = stale body) 3/3 on .63 and .61. Cause: ESPAsyncWebServer keeps ONE onDisconnect per request; REST routes call webArmSlotRelease() (REST/file gate, TASK-1172) after the body capture, which replaced the body-release callback. The host mock did not model the single slot nor the gate. Second fix (beta.60): webArmDisconnect(req, restHeld, fileHeld) is the one callback that releases gate slots AND the body, used by webCaptureBody() and webArmSlotRelease(); capture keeps a gate already held by the same request. Harness extension (single-slot mock + gate) in progress.
<!-- SECTION:NOTES:END -->
