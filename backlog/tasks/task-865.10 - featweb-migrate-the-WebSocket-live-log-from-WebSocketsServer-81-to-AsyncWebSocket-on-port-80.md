---
id: TASK-865.10
title: >-
  feat(web): migrate the WebSocket live-log from WebSocketsServer:81 to
  AsyncWebSocket on port 80
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-13 05:55'
updated_date: '2026-06-14 05:14'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.9
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 3; WS; depends seq9 = TASK-865.9)
The OT-log live-stream runs on a SEPARATE WebSocketsServer on TCP 81 (webSocketStuff.ino:36), polled via webSocket.loop() in handleWebSocket() (:246, 3 call sites). ADR-123 consolidates onto the built-in AsyncWebSocket attached to the port-80 AsyncWebServer, retiring port 81 and the loop poll.

## Critical fact
AsyncWebSocket is NOT a server: `AsyncWebSocket ws("/ws"); server.addHandler(&ws);`. Two listeners cannot bind port 80, so httpServer MUST already be AsyncWebServer (seq9, hard dep). Interface: seq9 exposes `extern AsyncWebServer server;`; this task does `server.addHandler(&otLogWs)` with `AsyncWebSocket otLogWs("/ws")` at path /ws on port 80.

## Firmware
- webSocketStuff.ino: webSocket -> AsyncWebSocket otLogWs("/ws"); drop include WebSocketsServer.h (here + networkStuff.h:47). startWebSocket() (230) -> addHandler + otLogWs.onEvent() ONCE; called 3x (OTGW-firmware.ino:362, networkStuff.ino:380, Ethernet.ino:146) -> make idempotent (no per-transition re-bind). webSocketEvent() (144, WStype_*) -> onEvent(...,AwsEventType,...) with WS_EVT_CONNECT/DISCONNECT/DATA/ERROR/PONG, preserving client-count, MAX_WEBSOCKET_CLIENTS=3 cap (client->close()), heap-reject (platformFreeHeap()<HEAP_WARNING_THRESHOLD), burst window (noteWebSocketBurstEvent 85-135). sendLogToWebSocket() (268) -> otLogWs.textAll(); PRESERVE the `&& canSendWebSocket()` heap gate (269, ADR-121 parity floor in OT-hot path; removing regresses the George MQTT-unavailable fix). sendWebSocketJSON() (219) -> otLogWs.textAll(); ~12 sites (OTDirect, OTGW-Core:5044/5081, SATcontrol:1999, SATcycles:533/568/1015). doWebSocketClose/DisconnectAll (42/50) -> otLogWs.closeAll(). hasWebSocketClients() (137) -> otLogWs.count()>0.
- handleWebSocket() retirement: webSocket.loop() removed; ADD periodic otLogWs.cleanupClients() (timer/residual loop); KEEP the 30s keepalive (252-257, KEEPALIVE_INTERVAL_MS, Safari ADR-025) via otLogWs.textAll keepalive JSON from a timer. Remove empty handleWebSocket() calls (OTGW-firmware.ino:745/756/806) - coordinate with seq9/11 via git add -p.
- platformio.ini: drop WebSockets @ 2.3.6 (37). ESP32Async deps owned by seq4.

## Web UI (data/index.js)
WEBSOCKET_PORT=81 (982) + wsURL `ws://host:81/` (1644) -> `ws://host/ws` (port 80, path /ws). Lifecycle (initOTLogWebSocket 1588, disconnect 1806, reconnect/watchdog/keepalive 1754) unchanged; keepalive JSON shape identical. KEEP ADR-025 Safari flashModeActive guard (1591) - re-validate, do not remove speculatively.

## Acceptance Criteria
- build: build.py compiles esp32/esp32-classic/esp32-combo with WebSockets removed + AsyncWebSocket via addHandler to the shared AsyncWebServer (grep per-env SUCCESS).
- evaluator: evaluate.py --quick no new failures; grep zero WebSocketsServer/webSocket.loop()/broadcastTXT/port 81 in firmware + WEBSOCKET_PORT/:81 in index.js.
- build: sendLogToWebSocket retains `&& canSendWebSocket()`; sendWebSocketJSON + ~12 sites route through otLogWs.textAll (port 80, /ws); startWebSocket idempotent across 3 sites.
- field: OT live-log streams to ws://host/ws (80) on real OTGW32 with a browser tab; reconnect after WiFi/Eth transition; 30s keepalive + cleanupClients keep idle alive.
- field: ADR-025 Safari upload-progress works (macOS+iOS) with WS on port 80; record whether close-before-flash mitigation is still needed; heap stays HEAPHEALTHY with live-log open for an extended session.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Landed: WebSocket live-log migrated from Links2004 WebSocketsServer:81 to AsyncWebSocket otLogWs("/ws") attached to the shared port-80 AsyncWebServer (ADR-133, supersedes ADR-005). startWebSocket() idempotent via wsInitialized; webSocketEvent ported to AwsEventHandler signature; connect-cap inverted to count() > MAX_WEBSOCKET_CLIENTS (lib inserts before dispatch); all TX via otLogWs.textAll()/closeAll(); wsClientCount removed (otLogWs.count() is sole source); webSocket.loop() removed, handleWebSocket() now 1s-timer cleanupClients()+keepalive; ADR-121 canSendWebSocket() heap gate and ADR-025 30s keepalive + flashModeActive guard preserved verbatim; index.js WEBSOCKET_PORT=81 -> WEBSOCKET_PATH='/ws' on window.location.host; WebSockets@2.3.6 dropped from platformio.ini. evaluate.py --quick green (0 fail). Remaining ACs are FIELD-VALIDATION on ESP32-S3 hardware: (1) OT live-log streams to ws://host/ws (80), reconnect across WiFi/Eth transition, 30s keepalive+cleanupClients keep idle alive; (2) ADR-025 Safari upload-progress on macOS+iOS, record whether close-before-flash mitigation still needed, heap stays HEAPHEALTHY over an extended live-log session.
<!-- SECTION:NOTES:END -->
