---
id: TASK-865.9
title: >-
  feat(web): migrate HTTP server, REST API and FSexplorer to ESPAsyncWebServer
  on port 80
status: To Do
assignee: []
created_date: '2026-06-13 05:54'
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
