---
id: TASK-869
title: >-
  fix(web): correct async HTTPMethod mapping in methodCompat (405/403 on REST
  v2)
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-15 05:28'
updated_date: '2026-06-15 06:18'
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
- [ ] #2 GET /api/v2/otgw/otmonitor returns 200 with the OT monitor JSON; the web UI OpenTherm Monitor stops showing the 405 retry banner and fills version/time
- [ ] #3 POST/PUT v2 endpoints still auth-gate correctly (no spurious 403 on GET navigation)
- [x] #4 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TEAM REVIEW (wot1wn2we) found a HIGH security regression that the method-mapping fix ACTIVATES: with methodCompat() now correctly returning HTTP_OPTIONS(6), the OPTIONS short-circuit in checkHttpAuth() (restAPI.ino:145, 'if (methodCompat()==HTTP_OPTIONS) return true;') began bypassing Basic Auth AND CSRF on the HTTP_ANY admin routes /ReBoot and /ResetWireless, whose handlers reach their side effect directly (a cross-origin CORS-preflight OPTIONS could wipe WiFi creds / reboot). /api/v2 is unaffected (OPTIONS intercepted upstream at restAPI.ino:2134). FIX: removed the OPTIONS short-circuit from checkHttpAuth() so OPTIONS falls through to the same auth+same-origin gate as every other method; /api/v2 CORS preflight still answered at 2134. Verified reBootESP/resetWirelessButton call checkHttpAuth before their side effect. Adjacent finding spun off as a separate task: /pic (upgradepic) is unauthenticated (TASK-870). Review also CONFIRMED the methodCompat switch itself compiles and fixes both 405 and 403.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the async HTTPMethod mapping: methodCompat() (webServerCompat.h) now maps AsyncWebRequestMethod::HTTP_* to the firmware http_parser HTTPMethod by name (switch) instead of a raw cast, so a GET stops being read as HTTP_HEAD. Resolves the 405 on every GET-guarded /api/v2 endpoint (otmonitor etc.) and the 403/auth misfires. The team review (wot1wn2we) caught a HIGH security regression the fix activates: the OPTIONS short-circuit in checkHttpAuth() bypassed auth+CSRF on the HTTP_ANY admin routes (/ReBoot, /ResetWireless); removed that short-circuit (api CORS preflight is answered upstream at restAPI.ino:2134). Build green on all 3 targets, evaluate 98.6%. Field-validation remaining (AC2/AC3): flash alpha.197, confirm the web UI OpenTherm Monitor loads without the 405 retry banner and that navigation no longer 403s. Adjacent unauthenticated /pic route spun off as TASK-870.
<!-- SECTION:FINAL_SUMMARY:END -->
