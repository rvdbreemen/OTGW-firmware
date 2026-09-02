---
id: TASK-1107
title: WebSocket server stops listening permanently after a WiFi reconnect
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-02 20:41'
updated_date: '2026-09-02 21:04'
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
- [x] #1 doWebSocketClose() is no longer used for the drop-stale-clients case; the WiFi-reconnect path drops clients only (webSocket.disconnect()) and leaves the listener bound
- [x] #2 The stale comment at networkStuff.ino claiming 'server keeps listening' is corrected to match actual library behaviour
- [ ] #3 After a simulated WiFi reconnect the device still accepts a WebSocket handshake on port 81 without a reboot
- [x] #4 prepareForReboot() keeps using the full close(), since the device restarts immediately afterwards
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Measured on 192.168.88.68 (fw 1.7.2-beta.3, 7d uptime): TCP connect to port 81 returns RST, while 192.168.88.16 (fw 1.7.5-beta.5) accepts the handshake. Primary source for the mechanism: libraries/WebSockets/src/WebSocketsServer.cpp:923-926 close() calls _server->close(); :948-949 loop() no-ops when _runnning is false; begin() is only called from OTGW-firmware.ino:176 in setup().

Fix: networkStuff.ino now calls doWebSocketDisconnectAll(), which maps to WebSocketsServerCore::disconnect() and only sends close frames to connected clients.

AC 3 still open: needs a device running this build through a real WiFi reconnect.
<!-- SECTION:NOTES:END -->
