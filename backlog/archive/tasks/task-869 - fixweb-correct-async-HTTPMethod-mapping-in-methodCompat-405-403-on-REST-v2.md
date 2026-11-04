---
id: TASK-869
title: >-
  fix(web): correct async HTTPMethod mapping in methodCompat (405/403 on REST
  v2)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-15 05:28'
updated_date: '2026-06-27 13:48'
labels: []
dependencies: []
ordinal: 85000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After the ESPAsyncWebServer migration (TASK-865.9), every REST /api/v2 endpoint with a method guard misbehaves: GET v2/otgw/otmonitor returns HTTP 405 (so the web UI OpenTherm Monitor shows 'Error: HTTP 405: Method Not Allowed (Retrying...)' and the version/time header placeholders never fill), and clicking around produces 403s. Root cause: webServerCompat.h methodCompat() raw-casts the ESPAsyncWebServer request method to the firmware HTTPMethod: (HTTPMethod)currentRequest->method(). The two enums share names but have DIFFERENT numeric values. ESPAsyncWebServer AsyncWebRequestMethod (namespaced, bitmask): DELETE=1<<0, GET=1<<1=2, HEAD=1<<2=4, POST=1<<3=8, PUT=1<<4=16, OPTIONS=1<<6=64. Firmware HTTPMethod = http_parser enum http_method (sequential): DELETE=0, GET=1, HEAD=2, POST=3, PUT=4, OPTIONS=6. So a browser GET (async 2) cast to HTTPMethod becomes http_parser HTTP_HEAD(2) != HTTP_GET(1) -> 405; POST/PUT land on the wrong constants -> auth/CSRF mismatches -> 403. Was latent until the TASK-866 GET / hang fix let the page load and fire the REST polls. Fix: map by name, not value, in methodCompat().
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 methodCompat() maps each AsyncWebRequestMethod::HTTP_* to the matching firmware HTTPMethod by name (switch), not a raw cast
- [x] #2 GET /api/v2/otgw/otmonitor returns 200 with the OT monitor JSON; the web UI OpenTherm Monitor stops showing the 405 retry banner and fills version/time
- [x] #3 POST/PUT v2 endpoints still auth-gate correctly (no spurious 403 on GET navigation)
- [x] #4 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TEAM REVIEW (wot1wn2we) found a HIGH security regression that the method-mapping fix ACTIVATES: with methodCompat() now correctly returning HTTP_OPTIONS(6), the OPTIONS short-circuit in checkHttpAuth() (restAPI.ino:145, 'if (methodCompat()==HTTP_OPTIONS) return true;') began bypassing Basic Auth AND CSRF on the HTTP_ANY admin routes /ReBoot and /ResetWireless, whose handlers reach their side effect directly (a cross-origin CORS-preflight OPTIONS could wipe WiFi creds / reboot). /api/v2 is unaffected (OPTIONS intercepted upstream at restAPI.ino:2134). FIX: removed the OPTIONS short-circuit from checkHttpAuth() so OPTIONS falls through to the same auth+same-origin gate as every other method; /api/v2 CORS preflight still answered at 2134. Verified reBootESP/resetWirelessButton call checkHttpAuth before their side effect. Adjacent finding spun off as a separate task: /pic (upgradepic) is unauthenticated (TASK-870). Review also CONFIRMED the methodCompat switch itself compiles and fixes both 405 and 403.

LIVE auth/CSRF validation on the connected esp32-classic (2026-06-20, temp password set then cleared, device restored to auth-off): unauth POST /api/v2/settings -> 401; authed Basic + cross-origin (Origin: evil) POST -> 403 (CSRF same-origin enforced, ADR-056); authed Basic same-origin POST -> 200. HTTP Basic Auth + CSRF same-origin enforcement on write routes confirmed working end-to-end. This closes the auth-enabled half of AC#3 (general write-path Basic Auth + CSRF). NOTE: I did NOT fire OPTIONS at /ReBoot or /ResetWireless (destructive); the OPTIONS-bypass-removal is covered indirectly by the same auth/CSRF machinery now proven on /api/v2/settings. AC#4 build receipt: 3-target build green at alpha.224+cdc4ec7 (esp32/esp32-classic/esp32-combo), evaluate.py --quick green (0 fail/1 warn/98.6%). Residual: AC#2 visual UI render (banner-gone + version/time header) not observed (browser MCP disconnected this session). Stays In Review pending that visual check.

On-device validation 2026-06-27 against OTGW32 @192.168.1.143 (fw 2.0.0-alpha.278+3d3f093, OT-Direct): AC#2 GET /api/v2/otgw/otmonitor -> HTTP 200 with otmonitor JSON; web UI OpenTherm Monitor shows 'Connected' streaming live OT frames, NO 405 retry banner, header fills version (2.0.0-alpha.278+3d3f093) and time (2026-06-27 15:36:42). AC#3 GET /api/v2/settings -> 200 (no spurious 403 on GET navigation); POST /api/v2/settings unauth -> 400 (handled, not 405/403). Browser console clean (no 405/403/retry/error). Pending: AC#4 fresh build confirmation (esp32 build in progress).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed async HTTPMethod mapping in methodCompat() (name-based switch, not raw cast). Validated on-device against OTGW32 ESP32-S3 @192.168.1.143 (fw 2.0.0-alpha.279+5b158a1, OT-Direct): GET /api/v2/otgw/otmonitor -> 200 JSON; web UI OpenTherm Monitor 'Connected' and streaming with NO 405 retry banner; header fills version+time. GET v2 -> 200 (no spurious 403), POST unauth handled (400, not 405/403). Build green esp32; evaluate.py --quick 0 failures.
<!-- SECTION:FINAL_SUMMARY:END -->
