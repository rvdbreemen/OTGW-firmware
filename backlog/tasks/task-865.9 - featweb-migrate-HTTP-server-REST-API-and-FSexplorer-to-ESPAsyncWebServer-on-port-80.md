---
id: TASK-865.9
title: >-
  feat(web): migrate HTTP server, REST API and FSexplorer to ESPAsyncWebServer
  on port 80
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-13 05:54'
updated_date: '2026-06-20 11:04'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.2
  - TASK-865.4
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 3; web; depends seq2=TASK-865.2, seq4=TASK-865.4)
Retire httpServer.handleClient() polling. Today the synchronous WebServer (networkStuff.ino:35, alias `using OTGWWebServer = WebServer` platform_esp32.h:39) drains 4 sockets/loop (OTGW-firmware.ino:812-815): one socket per handleClient() stretches a 6+ parallel-request page load across many loop turns (the sat-slider stall / XHR latency ramp). Move HTTP onto AsyncTCP's task. This task provides the `AsyncWebServer(80)` that seq10 (WS) and seq11 (OTA) attach to -> ordered before both. See other-projects/EMS-ESP32-dev for the async-server blueprint.

## Load-bearing design: pull vs push
REST is imperative push: sendStartJsonMap -> N x sendJsonMapEntry -> sendEndJsonMap (jsonStuff.ino:350/455) via restSendContent/restSendContentP/restSendP (230/241/254) calling httpServer.sendContent*. ESPAsyncWebServer is pull/callback. KISS bridge: re-target the restSend* layer to write into an AsyncResponseStream (request->beginResponseStream("application/json")) held in a file-static pointer set at handler entry, flushed with request->send(response) at exit. Preserves every sendJsonMapEntry body; only the 3 restSend* primitives + restSendP + the sendStartJsonMap header lines (jsonStuff.ino:359-362) change.

## Widen the chokepoint + convert ALL callers
Many handlers touch httpServer directly: sendApiError (restAPI.ino:65), sendApiMethodNotAllowed (70), sendCorsOriginHeader (44-51), processAPI (2089), checkHttpAuth (142) + isSameOriginRequest (107) -> thread AsyncWebServerRequest*. FSexplorer: serveVersionedAsset (75-95), sendIndex ?v=<hash> chunked (121-229), apifirmwarefilelist (334), apilistfiles (431), handleFile (520), handleFileUpload (538), doRedirect (649-693). ~200 call sites (restAPI 186, FSexplorer 124, jsonStuff 48).

## Static files: stream from flash
request->send(LittleFS, path, mime). Do NOT route ~39KB index.html through AsyncResponseStream. sendIndex ?v=<fsHash> keeps a chunked emitter ported to AsyncWebServerResponse chunked-callback. serveVersionedAsset keeps .gz-sibling + Cache-Control/ETag via response->addHeader.

## Routing + arg split (footgun)
Replace 27 httpServer.on(...) (FSexplorer startWebserver 98-275 + setupFSexplorer 282-331) with server.on(path,method,handler,onUpload,onBody) + server.onNotFound (the /api catch-all -> processAPI). Sync WebServer MERGES query+POST args in arg()/hasArg(); async SEPARATES (request->getParam(name,isPost)): 67 sites restAPI, 11 FSexplorer, 3 OTGW-Core -> each reviewed for GET-vs-POST + correct isPost. Highest bug density.

## Verify FIRST (state in plan before coding)
ESPAsyncWebServer/AsyncTCP serialize handler invocations on the single AsyncTCP task -> file-static URI/words buffers (restAPI.ino:2092-2094), originBuf/hostBuf, and the file-static AsyncResponseStream* stay safe. Verify via Context7/ESP32Async docs before committing; if handlers can run concurrently, escalate.

## Alias + loop edits
Retire `using OTGWWebServer = WebServer` (platform_esp32.h:39); expose `extern AsyncWebServer server;` (port 80) for seq10/11. Delete the 4x handleClient() drain (OTGW-firmware.ino:812-815) + the two in handleEspFlashBackgroundTasks (741)/handlePicFlashBackgroundTasks (751); seq10 removes adjacent handleWebSocket() lines - stage only your hunks (git add -p). LittleFS + feedWatchDog now run in handlers on the AsyncTCP limited stack (sendIndex 178, apifirmwarefilelist 414) - validate headroom on hardware.

## Acceptance Criteria
- build: esp32 + esp32-classic exit 0 with ESPAsyncWebServer+AsyncTCP linked (grep per-env SUCCESS); zero httpServer.handleClient() and zero httpServer. refs in OTGW-firmware.ino/FSexplorer.ino/restAPI.ino/jsonStuff.ino; sync WebServer include gone from networkStuff.h.
- build: OTGWWebServer alias removed from platform_esp32.h; no source references OTGWWebServer; single `extern AsyncWebServer server;` (port 80) exposed for seq10/11.
- evaluator: evaluate.py --quick no new failures (PROGMEM/String/abstraction green; no new raw #ifdef ESP32 outside allowlisted files).
- field: web UI loads on ESP32-S3 (index.html + 8 versioned assets ?v=<hash>); FSexplorer lists/deletes; all /api/v2 return correct JSON; HTTP Basic Auth + CSRF same-origin enforced on POST/PUT.
- field: page-load responsiveness no longer stalls under parallel requests (sat-slider tiles load); OT/MQTT throughput unaffected under web load; stack headroom confirmed for LittleFS handlers (sendIndex, apifirmwarefilelist) on the AsyncTCP task (no TWDT/stack-overflow during full FSexplorer walk).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
VERIFY FIRST (concurrency model, from installed lib headers — primary source, Context7 was 502):
AsyncTCP 3.4.10 runs ONE service task 'async_tcp' (CONFIG_ASYNC_TCP_RUNNING_CORE=-1, STACK_SIZE=16384, PRIORITY=10, QUEUE_SIZE=64). All lwIP events for all clients dispatch on this single task; ESPAsyncWebServer request handlers run on it, serialized (one handler runs to completion before the next event). => file-static buffers (currentRequest ptr, words[]/URI scratch, originBuf/hostBuf, the AsyncResponseStream* used by the restSend bridge) are SAFE. No concurrency escalation needed. CONFIG_ASYNC_TCP_USE_WDT=1 => long LittleFS walks on the async task are TWDT-watched (field validation, out of build scope).

DESIGN (KISS bridge, pull<-push):
1. networkStuff: OTGWWebServer httpServer(80) -> AsyncWebServer server(80). Remove OTGWWebServer alias from libraries/Platform/src/platform_esp32.h; networkStuff.h externs AsyncWebServer server.
2. OTA SEAM (per advisor): OTA is the SOLE consumer of the sync WebServer TYPE. Unwire httpUpdater.setup(&httpServer)+setIndex/setSuccess in networkStuff.ino (2 sites) with a TASK-865.11 marker. Leave OTGWUpdateServer object/extern/header/updateCredentials() untouched — header self-includes <WebServer.h>, compiles standalone, seq11 rewrites onto AsyncWebServer. OTA intentionally dark across the 865.9->865.11 seam (no OTA AC in 865.9).
3. restSend bridge (jsonStuff): file-static AsyncResponseStream* g_restStream set at handler entry; restSendContent/P, restSendP, sendStartJsonMap/Obj header lines write into it; request->send(stream) at exit. sendJsonMapEntry bodies unchanged.
4. Helpers: file-static AsyncWebServerRequest* currentRequest (set per route) instead of threading the ptr through ~330 sites + 15 handleX sigs. argCompat(req,name): getParam POST then GET — preserves sync arg() MERGED semantics exactly (per-site isPost is field-validation, not build-bar).
5. Static files: request->send(LittleFS,path,mime); serveVersionedAsset keeps .gz + Cache-Control/ETag via response addHeader; sendIndex ?v=<hash> -> chunked AwsResponseFiller emitter.
6. Routing: server.on(path,method,handler[,onUpload]) + server.onNotFound(processAPI catch-all). Convert OTGW-Core.ino(10) and OTDirect.ino(13) too (forced by build once instance/type gone; absent from AC grep-subset but in scope). SATble(2)=comments only.
7. Loop: delete 4x handleClient() drain + 2 in flash-bg helpers. Leave handleWebSocket() hunks for seq10 (git add -p at Land — not my concern, no commit here).

BUILD IN WAVES: skeleton (server, alias drop, restSend bridge, routing, 1 handler) per-env first, then convert in groups building after each. esp32 + esp32-classic must show per-env SUCCESS (grep log, build.py masks failures). evaluate.py --quick no new failures. No commit/bump/status (Land owns).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
REFINED DESIGN (advisor reconcile): split read vs write, NO transparent write-side shim.
READ SIDE (safe to shim on file-static currentRequest): argCompat(name)=getParam POST-then-GET (preserves sync arg() MERGED semantics), hasArgCompat, plus thin accessors for method/uri/header/hostHeader/authenticate. ~120 sites read like old API.
WRITE SIDE (explicit, 4-way rule, visible in diff):
 (a) small bounded JSON -> existing restSend* layer retargeted to file-static AsyncResponseStream* g_restStream (preserves every sendJsonMapEntry);
 (b) large/unbounded -> request->beginChunkedResponse(type,filler) (sendIndex 39KB, apifirmwarefilelist, apilistfiles, OTDirect chunked JSON, doRedirect) — must NOT buffer whole on fragmented S3 heap;
 (c) static file -> request->send(LittleFS,path,mime) (serveVersionedAsset, handleFile);
 (d) trivial -> request->send(code,type,body).
SEND-ONCE INVARIANT (the real bug-density, not GET/POST): ESPAsyncWebServer faults on double-send, hangs socket on no-send. Handlers have early returns (auth-fail 401, low-heap 500, 405) that must each be the SOLE finalize. Design: lazy-alloc g_restStream on first write; single g_responseSent flag; exactly one finalize point; every terminal helper (sendApiError, sendEnd* path, trivial sends) finalizes once + sets flag. This compiles green + passes evaluate.py + bites only in field => stated here for seq10/11.
DEFERRED for Land/field (recorded, not chased): per-site GET/POST isPost correctness; AsyncResponseStream RAM headroom for fattest JSON (sendOTmonitorV2); feedWatchDog vs async_tcp TWDT subscription; OTA dark across 865.9->865.11 seam.

IMPLEMENTED + BUILT (no commit per Land ownership). Files changed:
- NEW src/OTGW-firmware/webServerCompat.h — AsyncWebServer server(80) extern; per-request context (currentRequest, g_restStream, g_responseSent, g_pendingHeaders, g_requestBody); READ shims (argCompat/hasArgCompat/bodyCompat/headerCompat/hostHeaderCompat/methodCompat/uriCompat/authenticateCompat); WRITE helpers enforcing send-once (webSend/webSendStatus/webSendP/webSendFile/webRequestAuth/restBeginStream/restFinalize); webCaptureBody for onRequestBody.
- platform_esp32.h (libraries/Platform) — removed 'using OTGWWebServer = WebServer' alias (kept <WebServer.h> for OTA header).
- networkStuff.h — drop OTGWWebServer httpServer extern; include webServerCompat.h; AsyncWebServer server now extern there.
- networkStuff.ino — httpServer(80) -> AsyncWebServer server(80); define compat globals; UNWIRE httpUpdater.setup(&httpServer) x2 (OTA dark across 865.9->865.11 seam, marker left).
- OTGW-firmware.ino — delete 4x handleClient() drain loop + 2 in flash-bg helpers.
- FSexplorer.ino — full routing rewrite: server.on(...)+onNotFound+onRequestBody; sendIndex via beginChunkedResponse (no 39KB buffering); serveVersionedAsset/handleFile via request->send(LittleFS); apifirmwarefilelist/apilistfiles via g_restStream; handleFileUpload onUpload(request,fn,index,data,len,final); doRedirect via stream + requestDeferredReboot.
- restAPI.ino — ~180 sites converted: read shims, 4 chunked JSON blocks + wifi-scan + sendApiNotFound via g_restStream, body reads via bodyCompat, trivial sends via webSend/webSendP.
- jsonStuff.ino — restSend* layer retargeted to g_restStream; removed dead sTxBuf/restTxAppend/restFlushTxBuf coalescing block; CORS via webPushHeader.
- OTGW-Core.ino upgradepic + OTDirect.ino sendOTDirectOverridesJSON converted.
BUILD (per-env SUCCESS grepped, build.py masking accounted): esp32 SUCCESS Flash 97.4%; esp32-classic SUCCESS Flash 93.9%; AsyncTCP 3.4.10 + ESPAsyncWebServer 3.11.0 linked. evaluate.py --quick: 0 FAIL, 1 WARN (pre-existing 'boards.h not found' path artifact, unrelated), ESP-abstraction boundary PASS, no new raw platform #ifdef. Field ACs (browser/throughput/stack-headroom) out of scope (no hardware).

POST-REVIEW FIXES + FULL 3-ENV BUILD:
- FIX (advisor-caught defect): /upload POST completion handler sent the raw 'Header' HTTP literal ('HTTP/1.1 303 OK\r\nLocation:...') as a 200 HTML body (user would see raw bytes, no redirect). Replaced with sendFSexplorerRedirect() (proper 303+Location), matching the delete/format paths. Passed send-once but was wrong content. Added forward decl for static sendFSexplorerRedirect (used before its definition).
- Doc: added shared-static-buffer aliasing caution to webServerCompat.h READ section (argCompat/headerCompat/etc return one shared static; no two same-accessor calls in one expression).
- COMBO FLASH CHECK (advisor #1): this task links the async stack into esp32-combo for the FIRST time (combo previously had only the sync WebServer; ASYNC_LINK_PROOF is esp32-only). Combo went ~98.4% -> 99.0% Flash (1,945,511/1,966,080; ~20KB headroom). It FITS — no overflow. Tight but green; flagged here so Land knows combo is now near the ceiling (a future feature add may need a budget decision, not this task's scope).
FINAL BUILDS (all per-env SUCCESS, grepped): esp32 97.4%, esp32-classic 93.9%, esp32-combo 99.0%. evaluate.py --quick: 0 FAIL / 1 pre-existing WARN, 98.6% health. Build/evaluator ACs MET. Field ACs (browser load, FSexplorer list/delete, /api JSON, auth+CSRF, parallel-load responsiveness, stack headroom) require hardware — out of scope, deferred to Land/field.

LANDED (build+evaluator ACs met): HTTP server, REST API and FSexplorer migrated to ESPAsyncWebServer(80) over AsyncTCP; httpServer.handleClient() drain and OTGWWebServer alias removed; new webServerCompat.h bridges imperative-push JSON builders to async pull via file-static per-request context (safe under single AsyncTCP-task serialization) with send-exactly-once helpers and merged argCompat() semantics. ADR-132 (Proposed) supersedes ADR-109. Builds: esp32 97.4%, esp32-classic 93.9%, esp32-combo 99.0% (per-env SUCCESS grepped); evaluate.py --quick 0 FAIL. REMAINING field-validation ACs (require ESP32-S3 hardware, In Review): web UI + 8 versioned assets load; FSexplorer list/delete; all /api/v2 JSON correct; HTTP Basic Auth + CSRF same-origin on POST/PUT; sat-slider stall gone under parallel load; OT/MQTT throughput unaffected; AsyncTCP-task stack headroom for LittleFS handlers (no TWDT/stack overflow during full FSexplorer walk).

LIVE auth/CSRF validation on the connected esp32-classic (2026-06-20, temp password set then cleared, device restored to auth-off): unauth POST /api/v2/settings -> 401; authed Basic + cross-origin (Origin: evil) POST -> 403 (CSRF same-origin enforced, ADR-056); authed Basic same-origin POST -> 200. HTTP Basic Auth + CSRF same-origin enforcement on write routes confirmed working end-to-end. AC4 auth/CSRF half now LIVE-VALIDATED. AC1 build receipt: 3-target build green at alpha.224+cdc4ec7 (esp32/esp32-classic/esp32-combo), evaluate.py --quick green (0 fail/1 warn/98.6%). Residual gates remain (NOT closing): FSexplorer DELETE round-trip, OT/MQTT throughput under web load, AsyncTCP-task stack headroom / no-TWDT during full FSexplorer walk on ESP32-S3. Stays In Review.
<!-- SECTION:NOTES:END -->
