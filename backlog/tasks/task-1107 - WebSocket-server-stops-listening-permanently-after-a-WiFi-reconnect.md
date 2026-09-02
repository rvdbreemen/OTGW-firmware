---
id: TASK-1107
title: WebSocket server stops listening permanently after a WiFi reconnect
status: To Do
assignee: []
created_date: '2026-09-02 20:41'
labels: []
dependencies: []
ordinal: 202000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
refreshServicesAfterWifiReconnect() (networkStuff.ino:229) calls doWebSocketClose(), whose comment claims the server keeps listening. It does not: WebSocketsServer::close() (libraries/WebSockets/src/WebSocketsServer.cpp:923-926) calls _server->close() and sets _runnning=false, and nothing ever calls begin() again outside setup(). After one WiFi reconnect port 81 refuses all connections until reboot. Measured on device OTGW 192.168.88.68 (fw 1.7.2-beta.3, 7d uptime): port 81 TCP RST, while 192.168.88.16 (fw 1.7.5-beta.5) has 81 open. This also breaks the PIC flash button, which gates on the WebSocket being open.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 doWebSocketClose() is no longer used for the drop-stale-clients case; the WiFi-reconnect path drops clients only (webSocket.disconnect()) and leaves the listener bound
- [ ] #2 The stale comment at networkStuff.ino claiming 'server keeps listening' is corrected to match actual library behaviour
- [ ] #3 After a simulated WiFi reconnect the device still accepts a WebSocket handshake on port 81 without a reboot
- [ ] #4 prepareForReboot() keeps using the full close(), since the device restarts immediately afterwards
<!-- AC:END -->
