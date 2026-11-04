---
id: TASK-1172
title: >-
  REST in-flight slot leaks on file-streamed GETs (sensors/labels, sat/markers):
  webSendFile overwrites the onDisconnect release and /api/v2 wedges at 503
  until reboot
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 08:44'
updated_date: '2026-10-02 18:35'
labels:
  - bug
  - rest
  - webserver
dependencies: []
priority: high
ordinal: 295000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0 only)
Every GET of /api/v2/sensors/labels or /api/v2/sat/markers leaks one REST backpressure-gate slot (ADR-165) permanently, when two conditions hold:
- the backing file exists;
- the static-file gate admits the request.

Once restInFlight reaches the effective cap, every request that reaches processAPI() gets 503 "Server busy: too many concurrent requests, please retry" until reboot. On a device with DS18B20 sensors enabled, two classic-UI page loads are typically enough.

Affected builds:
- Introduced by 5a92b26e0 (2026-06-19, alpha.224).
- Present at HEAD 3621a3830 (alpha.377, per git show HEAD:version.h).
- src/ is identical between a26b19134 and HEAD.
- All ESP32 targets: esp32, esp32-classic and esp32-combo.

MECHANISM (every line re-read by the judge at HEAD)
1. ESPAsyncWebServer 3.11.0 keeps ONE disconnect callback per request, and the setter plain-assigns it.
   - The version is pinned at platformio.ini:212, and the same setter is in all three env libdeps.
   - ESPAsyncWebServer.h:435 `ArDisconnectHandler _onDisconnectfn;`
   - WebRequest.cpp:277-279 `void AsyncWebServerRequest::onDisconnect(ArDisconnectHandler fn) { _onDisconnectfn = fn; }`
   - WebRequest.cpp:281-286 runs only that one callback when the connection closes. The server closes after every response (WebRequest.cpp:255-264).
2. processAPI() (restAPI.ino:2691) takes the slot and registers the release BEFORE handler dispatch (dispatch is at :2786).
   - :2712 `restInFlight++;`
   - :2714 `request->onDisconnect([]() { if (restInFlight) restInFlight--; });`
3. The handler then calls webSendFile() on the same request.
   - currentRequest is bound by webBeginRequest at restAPI.ino:2695 and webServerCompat.h:114.
   - After the file gate admits (webServerCompat.h:322), webServerCompat.h:334 runs `currentRequest->onDisconnect([]() { webFileGateRelease(); });`
   - That assignment replaces the REST release, so restInFlight is never decremented for this request.
   - Nothing else writes restInFlight: only restAPI.ino:46, :2712 and :2714. Telnet 'z' resets only the high-water mark (helperStuff.ino:458).
4. The invariant comment at restAPI.ino:2700-2702 ("request->onDisconnect() fires exactly once per request -> the counter is balanced and cannot leak") has been false since 5a92b26e0.
   - That commit added the file-gate registration on top of the REST gate from d23c87911 (2026-06-18).
   - Both routes have used webSendFile since 6601c29fc.

CALL SITES
The original claim had the two line numbers swapped.
- GET /api/v2/sensors/labels: handleSensors (restAPI.ino:450-452) -> getDallasLabels (:4230-4239) -> webSendFile at :4238. If /dallas_labels.ini is missing, :4231-4233 answers "{}" and nothing leaks.
- GET /api/v2/sat/markers: handleSAT (:1111, auth at :1113) -> markers branch (:1730) -> webSendFile at :1763. If /sat_markers.json is missing, :1759-1760 answers "[]" and nothing leaks.
- Both routes are registered unconditionally (:2530, :2542).

Paths that do NOT leak:
- A REST-gate 503 (:2705-2710) returns before the slot is taken.
- A file-gate 503 (webServerCompat.h:322-332) returns before :334, so the REST callback survives.
- checkApiRateLimit (:2668-2687) covers only otgw/otmonitor, otgw/telegraf and device/time.

The missing-file 404 inside webSendFile (:338-343) runs after :334, so a file deleted between the caller's exists() check and webSendFile also leaks. This is a race only.

No other processAPI-reachable code calls webSendFile. The remaining callers are static-asset routes and handleFile, which are reached only for non-/api/ paths (FSexplorer.ino:343-354).

WEDGE THRESHOLD
restEffectiveInflightCap is at restAPI.ino:55-64, with REST_MAX_INFLIGHT 2 at :44.
- Cap 2 (maxblock >= 16000):
  - One leak leaves REST single-flight, so the classic UI's parallel init fetches (index.js:3554-3575) start drawing 503s.
  - Two leaks: 503 on every processAPI request until reboot.
- Cap 1 (maxblock < 16000, :61):
  - One leak blocks REST while maxblock stays low.
  - REST returns to single-flight if maxblock recovers.

SCOPE
Hit: everything routed through processAPI.
- /api/v2/*, including OPTIONS preflights, because the gate at :2705 runs before the OPTIONS branch at :2756.
- The /api/v0 and /api/v1 410 path.

Not hit:
- /api/firmwarefilelist and /api/listfiles. Their own handlers are registered first (FSexplorer.ino:300-301 via setupFSexplorer at OTGW-firmware.ino:486), before the /api route (FSexplorer.ino:238 via startWebserver at :487), and the first match wins (WebServer.cpp:145-150).
- Static assets, /ws, MQTT/HA, the OT gateway and the SAT control loop.

Recovery: /ReBoot or a power cycle. The wedge returns after the next leaking GETs.

TRIGGERS
- The default classic UI (UItypes.h:27 `bool bUseV2 = false;`) fetches labels on every page load: the load hook (index.js:151-160) -> initMainPage (:3346) -> fetchDallasLabels() at :3554 -> fetchWithRetry(APIGW + 'v2/sensors/labels') at :107.
  - A 503 never leaks, but a retry the gates later admit does.
  - Other labels GETs:
    - index.js:4175 (caller chain not re-read by the judge);
    - index.js:5822-5825, the sensor-simulation refetch;
    - index.js:9287-9289, the label editor.
  - External REST clients polling this endpoint leak the same way.
- /dallas_labels.ini appears without user action. ensureSensorDefaultLabels() writes it (sensors_ext.ino:69) in three cases, and it persists afterwards:
  - at boot, when sensors are enabled and at least one DS18B20 is detected (sensors_ext.ino:193-200);
  - in sensor simulation (:143-146);
  - on any labels POST/PUT (restAPI.ino:4291).
- /sat_markers.json exists after the first marker POST (rename at restAPI.ino:1833). DELETE rewrites it and never removes it (:1851-1874). From then on, each of these leaks:
  - the classic SAT page open (sat.js:763);
  - the reload after each marker add or delete (sat.js:1385, :1402), with the fetch at sat.js:1286;
  - the first v2 SAT page open per session (v2.js:3048, fetch at :3108).
- Not exposed: a default device with sensors disabled, no simulation, no saved labels and no markers.

IMPACT
- The web UI shows no data, and every settings read or save fails, until reboot.
- Gateway, MQTT and SAT control keep running. There is no crash and no safety impact.
- The failure is deterministic, not timing-dependent.
- No field report found. The Discord MCP was down. A backlog search for onDisconnect, restInFlight, "in-flight leak", "Server busy" and webSendFile found no covering task.

1.x STATUS: not present, no port needed.
- otgw-1.x.x (wt-otgw-1.x.x @ 8ba6f7ef9) uses the synchronous ESP8266WebServer.
- A grep of src/OTGW-firmware for onDisconnect|restInFlight|REST_MAX_INFLIGHT|webFileGate|AsyncWebServer returns 0 matches.
- 1.x getDallasLabels streams via streamFileGuarded (restAPI.ino:1752, FSexplorer.ino:75), which has no in-flight counter.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Code review against the library's single-slot setter (WebRequest.cpp:277-279) shows two properties. Every processAPI request that increments restInFlight releases exactly one REST slot on disconnect, including requests whose handler streams a file through webSendFile. Every webSendFile admission releases exactly one file-gate slot. This holds on all four paths: the REST 503 (restAPI.ino:2705-2710), the file-gate 503 (webServerCompat.h:322-332), the missing-file 404 inside webSendFile (:338-343, verified by review only because the race cannot be triggered on demand) and the normal stream (:345-349).
- [x] #2 OLD-vs-FIX bench reproduction, labels route, same board and same sequence. Flash app-only (flash_otgw.bat --update --app) so /dallas_labels.ini survives, and send strictly sequential curl requests.

Setup: reboot, press telnet 'z', POST {"28D0000000000001":"bench"} to /api/v2/sensors/labels, and read internal_maxblk from GET /api/v2/device/info.

Sequence:
1. GET labels
2. GET device/info
3. GET labels
4. GET device/info 3x, 1 s apart
5. OPTIONS /api/v2/health
6. Wait 60 s, then GET device/info

OLD at cap 2 (internal_maxblk >= 16000): the first device/info after one labels GET reports hd_rest_inflight_hwm=2 under strictly sequential traffic. Every later /api/v2 call returns 503 "Server busy: too many concurrent requests, please retry", still after 60 s, and telnet (REST debug on) logs "REST BUSY: 2/2 in-flight (cap 2)".

OLD at cap 1: the first device/info after one labels GET already returns 503.

FIX: every call succeeds (200; the OPTIONS preflight gets its normal 204), hd_rest_inflight_hwm stays 1, and hd_rest_503 stays 0.
- [x] #3 Same old-vs-fix reproduction for GET /api/v2/sat/markers, after one POST {"outside_temp":5,"flow_temp":55,"label":"bench"} to /api/v2/sat/markers, with a reboot before the OLD markers run. OLD shows the same hwm=2 / persistent-503 signature. FIX stays at 200.
- [x] #4 FIX soak: run 20 sequential GET /api/v2/sensors/labels and 20 sequential GET /api/v2/sat/markers, then GET /api/v2/device/info. device/info returns 200 with hd_rest_inflight_hwm <= 1, hd_webfile_inflight_hwm <= 1, hd_rest_503 = 0 and hd_webfile_503 = 0.
- [x] #5 FIX, refusal paths stay balanced. Run a mixed burst (2 concurrent GET /index.js together with 2 concurrent GET /api/v2/sensors/labels, repeated until hd_webfile_503 or hd_rest_503 is non-zero). Then press telnet 'z' and run the sequential probe: device/info, labels, device/info. All three return 200, with hd_rest_inflight_hwm = 1 and hd_webfile_inflight_hwm = 1, so no slot is left held.
- [x] #6 FIX, classic UI end-to-end, with a browser devtools capture (capture-mqtt-debug.bat CDP):
- With /dallas_labels.ini present, 10 consecutive reloads keep showing live data, and no /api/v2 request returns 503 once each load has settled.
- With a marker present, opening the classic SAT page 5 times and adding and deleting one marker keeps /api/v2 at 200.
- [x] #7 The fix edits nothing under .pio/libdeps or src/libraries. A grep shows that application code calls ->onDisconnect( only through the single compat-layer helper in webServerCompat.h, apart from the existing OTA abort hook in OTGW-ModUpdateServer-esp32.h. The stale comments at restAPI.ino:2700-2702, restAPI.ino:2778 and webServerCompat.h:311 are corrected to describe the one-callback-per-request rule.
- [x] #8 build.bat for esp32-combo prints its SUCCESS line and produces a fresh firmware.bin. python evaluate.py --quick shows no new failures. The change lands in one commit together with its own prerelease bump (bin/bump-prerelease.sh).
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from the backlog triage (workflow wf_1c568508-5f2) after an adversarial verification (workflow wf_4f2df5e8-285): 3 independent verifiers (trace / refute / impact) all voted real; judge severity high.

FIX PLAN:
Root cause: two gates share the library's single per-request disconnect slot. The fix gives that slot one owner in the compat layer. Each registration releases every gate the request holds so far. The library is pinned upstream and is not edited.

Recommended fix, a webHoldSlot bitmask (about 15 lines):
1. webServerCompat.h, next to the per-request externs (:53-55), add `extern uint8_t g_heldSlots;`. In webBeginRequest() (:113-118), add `g_heldSlots = 0;`.
   - This reset is safe because every webBeginRequest caller is a route entry point: the FSexplorer.ino route lambdas and handlers, formatLittleFS :721, reBootESP :776, resetWirelessButton :784, upgradepic OTGW-Core.ino:6110, OTGW-ModUpdateServer-esp32.h:118/134, and processAPI restAPI.ino:2695.
   - processAPI's own call runs before the slot is taken, and restAPI.ino calls none of the others mid-dispatch.
2. webServerCompat.h, after :311, add:
   - `void restGateRelease();`
   - `static constexpr uint8_t WEB_SLOT_REST = 0x01, WEB_SLOT_FILE = 0x02;`
   - `inline void webHoldSlot(uint8_t slot) { g_heldSlots |= slot; const uint8_t held = g_heldSlots; currentRequest->onDisconnect([held]() { if (held & WEB_SLOT_REST) restGateRelease(); if (held & WEB_SLOT_FILE) webFileGateRelease(); }); }`
   - The bitmask is captured by value because the callback runs after the file-static context has moved on. A 1-byte trivially copyable capture is stored in place by std::function, so there is no heap allocation.
3. webServerCompat.h:334: replace the registration with `webHoldSlot(WEB_SLOT_FILE);`.
4. networkStuff.ino:45-48, next to currentRequest (ADR-044 single instantiation): define `uint8_t g_heldSlots = 0;`.
5. restAPI.ino:
   - After :46, add `void restGateRelease() { if (restInFlight) restInFlight--; }`. It needs external linkage, like webFileGateRelease at :100.
   - Replace :2714 with `webHoldSlot(WEB_SLOT_REST);`. currentRequest is the same object as request, bound at :2695.
6. Correct the comments at restAPI.ino:2700-2702 and :2778 and at webServerCompat.h:311.
7. Run bin/bump-prerelease.sh and commit it together with the change.

Why every path stays balanced:
- A REST 503 returns before the slot is taken and registers nothing.
- A file-gate 503 returns before webHoldSlot(FILE), so the REST-only callback stays in place.
- The missing-file 404 and the normal stream both run under the combined callback.
- Static-asset routes hold only FILE, so their behaviour does not change.
- The /update abort hook (OTGW-ModUpdateServer-esp32.h:257) sits on a route that never calls webSendFile and stays as it is.

Smaller alternative, for the maintainer's KISS call: add a per-request `bool g_restSlotHeld`, set right after restInFlight++ and cleared in webBeginRequest. webSendFile then registers `[rest]() { webFileGateRelease(); if (rest) restGateRelease(); }`. It has the same effect for today's two gates but does not protect against a third registrant.

Rejected:
- Exempting the REST routes from the file gate: this drops ADR-147 D4.1's FD-allocation bound for these streams.
- Patching the library: it is a pinned upstream library and out of scope.
- Moving the REST registration after dispatch: that would drop the file-gate release instead.

Optional: an evaluate.py check that `->onDisconnect(` in application code appears only in the webServerCompat.h helper and in OTGW-ModUpdateServer-esp32.h. Under ADR-080, add that gate or label the rule guideline-level.

REPRO PLAN:
Proof has to come from an old-vs-fix run on the bench. A host test cannot reach the defect: the single slot lives in the ESPAsyncWebServer runtime (WebRequest.cpp:277-287), the REST registration is inline in processAPI, and a stub request would only re-implement the library. Run this after the current bench test finishes; no device was contacted for this analysis.

SETUP
- Hardware: the OTGW32 bench S3 (the classic-S3 board also works; the defect does not depend on the board). No DS18B20, PIC, boiler or thermostat is needed.
- Tools: curl, telnet on port 23, flash_otgw.bat, and optionally scripts/capture-mqtt-debug.bat for one transcript.
- This assumes no admin password is set; otherwise add -u admin:<pw>.
- Save response bodies with -o <file> -w "%{http_code}". Never use -o /dev/null.
- Builds:
  - OLD is dev HEAD 3621a3830 (alpha.377). Confirm the running version with GET /api/v2/device/info first, because boards can lag committed code.
  - FIX is HEAD plus the patch under its own prerelease tag.
- Flash app-only: flash_otgw.bat --board esp32 --update --app <bin>. A filesystem flash wipes /dallas_labels.ini and /sat_markers.json and masks the bug.

OLD RUN
1. curl http://<ip>/ReBoot and wait for the boot to finish. In telnet, press '2' once (it toggles REST debug), then 'z' (zeroes the rest and webfile high-water marks and the 503 counters, helperStuff.ino:458-461).
2. Create the precondition without touching MQTT/HA: curl -X POST -H "Content-Type: application/json" -d '{"28D0000000000001":"bench"}' http://<ip>/api/v2/sensors/labels should return 200 (it writes the file, restAPI.ino:4291). Do not use telnet 'd': sensor simulation publishes fake sensors.
3. GET /api/v2/device/info and note internal_maxblk (restAPI.ino:3236). At 16000 or above the cap is 2; below it the cap is 1.
4. Send these requests strictly one after another:
   - GET /api/v2/sensors/labels: 200, with the file as the body.
   - GET /api/v2/device/info: at cap 2, 200 with hd_rest_inflight_hwm=2 (the leading indicator; device/info itself takes a slot). At cap 1, 503.
   - GET labels again: 200 at cap 2.
   - GET device/info 3 times, 1 s apart.
   - curl -X OPTIONS http://<ip>/api/v2/health.
   - Wait 60 s, then GET device/info.
5. Expected on OLD:
   - The tail of step 4 returns 503 with {"error":{"status":503,"message":"Server busy: too many concurrent requests, please retry"}}, and it persists after 60 s.
   - Telnet logs "REST BUSY: 2/2 in-flight (cap 2) => 503".
   - Telnet 'D' shows rest_503 rising (handleDebug.ino:203).
   - GET /api/listfiles and GET /index.js still return 200, because they bypass processAPI.
   - Recover with /ReBoot.
6. Markers variant, after a reboot: POST {"outside_temp":5,"flow_temp":55,"label":"bench"} to /api/v2/sat/markers (expect 201), then run the step 4 sequence with GET /api/v2/sat/markers in place of labels.
7. Optional browser check: with the labels file present, reload the classic UI twice. DevTools shows every /api/v2 call returning 503.

FIX RUN
- Flash FIX app-only and repeat steps 1-7.
- Expected: every call returns 200 (the OPTIONS preflight gets its normal 204), hd_rest_inflight_hwm stays 1, hd_rest_503 stays 0, and no REST BUSY lines appear.
- Then run, as described in the ACs:
  - the soak: 20 labels GETs plus 20 markers GETs, then device/info;
  - the mixed-burst balance check: 'z' followed by the sequential probe;
  - 10 classic-UI reloads under a CDP devtools capture.

CLEANUP
- If the test files did not exist before, delete /dallas_labels.ini and /sat_markers.json through FSexplorer. A marker DELETE only rewrites the file to [].
- Toggle telnet '2' off.

DISSENT / CORRECTIONS:
None. All three verifiers (trace, refute, impact) returned real with high confidence. The refute lens found no refutation. The judge independently re-read every decisive line and confirmed the chain:
- the plain-assignment setter at WebRequest.cpp:278;
- the REST registration before dispatch at restAPI.ino:2714;
- the overwrite at webServerCompat.h:334;
- no other writer of restInFlight.

Detail disagreements, all resolved:
1. The claim's line numbers for the two routes are swapped: :1763 is markers and :4238 is labels. All three verifiers flagged this, and the judge confirmed it.
2. "Every /api/*" is too broad. Verifier 3 left the /api/firmwarefilelist and /api/listfiles bypass unverified. The judge confirmed it from the first-match loop at WebServer.cpp:145-150 and the registration order at OTGW-firmware.ino:486-487.
3. Retry wording. Verifier 1 said a retried labels GET "usually leaks anyway"; verifier 2 said fetchWithRetry causes no extra leaks. Both are consistent: a 503 never leaks, and a retry that the gates later admit leaks like a first attempt.
4. Verifier 3 cited HEAD a26b19134, while the current HEAD is 3621a3830 (alpha.377). git diff a26b19134 HEAD -- src/ is empty, so the evidence is unchanged.
5. Verifier line numbers for UI callers were re-anchored by the judge:
   - labels fetch at index.js:9287-9289;
   - markers fetches at sat.js:1286 and v2.js:3108;
   - markers call sites at sat.js:763/1385/1402 and v2.js:3048.
6. Verifier 3 noted, and the judge confirmed, that the missing-file 404 inside webSendFile also leaks. It is a race only.

2026-09-30 implementation (alpha.379).
Design: one compat-layer helper, webArmSlotRelease() in webServerCompat.h, arms the request's single disconnect callback for exactly the gate slots it holds (flags g_restSlotHeld / g_fileSlotHeld, both reset in webBeginRequest(), defined once in networkStuff.ino per ADR-044). processAPI() sets g_restSlotHeld after restInFlight++ and calls the helper; webSendFile() sets g_fileSlotHeld after the file gate admits and calls it again, which re-arms ONE callback releasing both. Chosen over a gate=false parameter for REST file routes because that would let the next REST route that streams a file reintroduce the leak silently.
AC#1 review of the four paths (single-slot setter WebRequest.cpp:277-279):
- REST 503 (restAPI.ino ~:2707): returns before restInFlight++ and before arming: nothing held, nothing to release.
- file-gate 503 (webSendFile): returns before g_fileSlotHeld is set; the REST-only callback armed by processAPI() stays, so the REST slot is released; for a static asset nothing is held.
- missing-file 404 inside webSendFile: after the combined callback is armed, so both slots are released on disconnect.
- normal stream: combined callback, both released.
webSendFile() runs at most once per request (g_responseSent guard), so a slot is never counted twice.
AC#7: grep '->onDisconnect(' in src/OTGW-firmware: only webServerCompat.h webArmSlotRelease() (3 branches) and the OTA abort hook OTGW-ModUpdateServer-esp32.h:257. Stale comments corrected: restAPI.ino processAPI gate comment, restAPI.ino ADR-172 placement comment, webServerCompat.h gate forward declarations. Nothing under .pio/libdeps or src/libraries changed.
AC#8: bin/bump-prerelease.sh alpha.378 -> alpha.379 in this commit; build.bat --target esp32-combo [SUCCESS] firmware (181.2 s) + filesystem, fresh firmware.bin 11:05:30 / littlefs.bin 11:06:03 (alpha.379+0976995, image saved under %LOCALAPPDATA%/OTGW-capture/img-alpha379-0976995); evaluate.py --quick 69 passed, 0 warnings, 0 failed.
OPEN: AC#2-#6 bench (OTGW32): OLD = IMG-0 alpha.377+3621a38 (saved), FIX = alpha.379 or later; app-only flash so /dallas_labels.ini survives.

2026-10-02 bench run, OTGW32 (COM4, 192.168.88.61).

Setup:
- OLD = IMG-0 2.0.0-alpha.377+3621a38 (combo). FIX = 2.0.0-alpha.397+0fdb1e5 (combo). Both flashed app-only with flash_otgw.bat --update --app, version confirmed with GET /api/v2/device/info.
- internal_maxblk 31732-40948, so the cap is 2.
- Strictly sequential requests (t1172.py). Telnet '2' (REST debug, the echo verified 'true') and 'z' before each run.
- Evidence in %LOCALAPPDATA%/OTGW-capture/task1172-bench-20261002/.

AC#2, labels route, OLD vs FIX, same board, same sequence:
- OLD: POST labels 200 -> device/info 200 -> GET labels 200 -> device/info 200 with hd_rest_inflight_hwm=2 -> GET labels 200 -> 3x device/info 503 'Server busy' -> OPTIONS /api/v2/health 503 -> after 60 s, device/info still 503. /api/listfiles and /index.js 200 (they bypass processAPI). 5 telnet lines 'REST BUSY: 2/2 in-flight (cap 2) => 503'.
- FIX: every call 200, OPTIONS 204. hd_rest_inflight_hwm stays 1, hd_rest_503 0, no REST BUSY line.

AC#3, markers route, after a reboot, POST {outside_temp 5, flow_temp 55, label bench} = 201:
- OLD: the same signature. hwm=2 at step 4b, then 503 on 3x device/info, on OPTIONS and after 60 s. 5 REST BUSY lines.
- FIX: all 200 / 201 / 204, hwm 1, 503 0, no REST BUSY.

AC#4, FIX soak after 'z':
- 20 sequential GET labels and 20 sequential GET markers all 200.
- device/info 200 with hd_rest_inflight_hwm 1, hd_webfile_inflight_hwm 1, hd_rest_503 0, hd_webfile_503 0.

AC#5, FIX refusal paths:
- The AC's mixed burst (2x /index.js plus 2x labels) refused both index.js requests in round 1 (hd_webfile_503 2, 'WEBFILE BUSY' x2). After 'z' the probe device/info, labels, device/info returned 200, 200, 200 with hwm rest 1 and webfile 1.
- Extended with the two paths that matter for the leak (t1172c.py), each followed by 'z' and the same probe, both with hwm 1/1:
  - (A) file-gate 503 on a REST route: 2x index.js first, labels 150 ms later. Both labels requests got 503 from the file gate while holding a REST slot.
  - (B) REST-gate 503: 4x labels at once, 2 REST BUSY.

AC#6, FIX classic UI. Playwright on headless Edge records every response and console message (CDP), in place of capture-mqtt-debug.bat. The matching alpha.397 LittleFS was flashed first; settings were identical before and after. Labels file and one marker were re-created by POST.

Part 1, initial load plus 10 reloads, all pass:
- Every load: GET /api/v2/sensors/labels 200; 4-6 otmonitor polls 200 with otmonitor data (live data); 0 /api/v2 503s after the 5 s settle point.
- 2-6 transient 503s inside each load's first seconds (device/info, device/time, filesystem/hash-check). The classic UI fires 6 API requests at once on load, a browser peak of 7 in flight.

Part 2 does NOT pass as written, because of two defects outside this task:
- (a) The classic SAT page start fires about 5 API calls at once. On 4 of 5 opens GET /api/v2/sat/markers itself got 503, and loadMarkers() has no retry, so the marker list stayed empty. Console: '[SAT] marker load error: Service Unavailable' x4, also sat/status and sat/weather 503 x4-5, device/time 429 x6.
- (b) A click on the heating-curve chart never adds a marker. The chart rendered at 1368x320 with an ECharts instance, and grid point (5, 50) maps to pixel (688, 128). A real mouse click there sent no POST. The instance-level on('click') handler (sat.js _initCurveClickHandler) receives no event from a background click; only getZr() sees it. TASK-586 AC#1 was checked without that path working.

No leak under the UI traffic: after the whole UI session, 'z' plus the sequential probe gave device/info hwm rest 1 / webfile 0, labels 200, device/info hwm rest 1 / webfile 1, 503 0.

AC#6 stays open until (a) TASK-1193 and (b) TASK-1192 are fixed.

Cleanup pending: /dallas_labels.ini and /sat_markers.json were created for the test. They are kept until AC#6 can be re-run, then removed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
REST in-flight slot leak on file-streamed GETs.

Cause: ESPAsyncWebServer keeps one disconnect callback per request. webSendFile()'s file-gate registration overwrote processAPI()'s REST release, so every GET of /api/v2/sensors/labels or /api/v2/sat/markers with an existing backing file leaked one REST slot. Two leaks wedged /api/v2 at 503 until reboot.

Fix (alpha.379): one compat-layer helper, webArmSlotRelease(), arms a single callback that releases every gate slot the request holds.

Evidence per AC. Bench runs on the OTGW32, 2026-10-02. Captures in %LOCALAPPDATA%/OTGW-capture/task1172-bench-20261002/.
- AC#1 (review of the four paths) and AC#7 (no library edits, one onDisconnect helper): code review recorded at implementation.
- AC#8: alpha.379 build and evaluate, recorded at implementation.

AC#2 (labels) and AC#3 (markers), old vs fix, app-only flashes, strict sequential runs (t1172.py):
- OLD = IMG-0 alpha.377+3621a38. GET route 200, then device/info 200 with hd_rest_inflight_hwm=2, then a second GET 200. After that every /api/v2 call answered 503, OPTIONS included, still after 60 s. 5 telnet 'REST BUSY: 2/2 in-flight' lines per route.
- FIX = alpha.397: all 200, OPTIONS 204, hwm 1, 503 counters 0, no REST BUSY.

AC#4: 20 labels GETs and 20 markers GETs, all 200. Then device/info 200 with hwm rest 1 / webfile 1 and 503 counters 0.

AC#5:
- The mixed burst (2x /index.js + 2x labels) refused both index.js requests on the file gate.
- Extended with the paths that matter for the leak: (A) 2 labels requests refused by the file gate while holding a REST slot; (B) REST-gate refusals from 4 concurrent labels GETs.
- After telnet 'z' each time, the sequential probe device/info, labels, device/info returned 200 with hwm rest 1 / webfile 1.

AC#6, classic UI end to end. Playwright drives headless Edge over CDP in place of capture-mqtt-debug.bat. Final run on alpha.400 (t1172ac6-400-capture.json):
- 11 loads with /dallas_labels.ini present: labels 200 each, 6 live otmonitor polls each, zero 503 or 429.
- With a marker present: one marker added by clicking the heating curve (201) and deleted with its x button (200), then 5 thermostat page opens. Every /api/v2 response was 2xx and the markers GET was 200 on each open.
- The balance probe after 'z' gave hwm rest 1 / webfile 1.
- The first AC#6 run on alpha.397 exposed separate classic UI defects. TASK-1192 (curve click, vanishing markers) and TASK-1193 (request burst past the cap, ADR-184) were fixed and verified before this run.
- TASK-1194 (blank charts after Home -> SAT) was found as well. It does not affect this AC, because the marker was added on a fresh open, and it is handled in its own task.

The test files /dallas_labels.ini and /sat_markers.json were removed afterwards through /api/listfiles?delete=. The API answers {} and [] again.
<!-- SECTION:FINAL_SUMMARY:END -->
