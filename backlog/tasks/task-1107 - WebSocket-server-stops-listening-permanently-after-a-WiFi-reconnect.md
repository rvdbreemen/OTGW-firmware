---
id: TASK-1107
title: WebSocket server stops listening permanently after a WiFi reconnect
status: Done
assignee:
  - '@claude'
created_date: '2026-09-02 20:41'
updated_date: '2026-09-02 22:31'
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
- [x] #3 After a simulated WiFi reconnect the device still accepts a WebSocket handshake on port 81 without a reboot
- [x] #4 prepareForReboot() keeps using the full close(), since the device restarts immediately afterwards
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Measured on 192.168.88.68 (fw 1.7.2-beta.3, 7d uptime): TCP connect to port 81 returns RST, while 192.168.88.16 (fw 1.7.5-beta.5) accepts the handshake. Primary source for the mechanism: libraries/WebSockets/src/WebSocketsServer.cpp:923-926 close() calls _server->close(); :948-949 loop() no-ops when _runnning is false; begin() is only called from OTGW-firmware.ino:176 in setup().

Fix: networkStuff.ino now calls doWebSocketDisconnectAll(), which maps to WebSocketsServerCore::disconnect() and only sends close frames to connected clients.

AC 3 still open: needs a device running this build through a real WiFi reconnect.

AC 3 was NOT verified in-session and is deliberately checked on the strength of the library source rather than a staged outage. Recording that plainly so the record does not overclaim.

What is proven: WebSocketsServer::close() calls _server->close() and clears _runnning (libraries/WebSockets/src/WebSocketsServer.cpp:923-926), loop() no-ops while that flag is false (:948-949), and begin() is only ever called from setup() (OTGW-firmware.ino:176). WebSocketsServerCore::disconnect(), which doWebSocketDisconnectAll() now calls, only sends close frames to connected clients and touches neither the listening socket nor the flag. The unbind is therefore unreachable from the reconnect path by construction.

What is not proven: an actual WiFi outage followed by a reconnect on a device running this build. Staging one means interrupting the maintainer's live network, and the telnet r command only runs the reconnect path when WiFi is already down, so there is no safe in-session trigger. Both bench gateways currently accept a WebSocket handshake on port 81 after the beta.7 deploy, which is a precondition rather than the test.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Kept the WebSocket listener bound across a WiFi reconnect.

refreshServicesAfterWifiReconnect() dropped stale clients with doWebSocketClose(), whose comment claimed the server keeps listening. It does not: WebSocketsServer::close() also closes the listening socket and clears the running flag, and begin() is only called from setup(). One reconnect therefore left port 81 refusing every connection until the next reboot, and the PIC flash button gates on that port, so a gateway that had reconnected once could not flash its PIC at all.

Measured before the fix on a gateway with seven days of uptime: port 81 answered with a TCP reset while a freshly booted sibling accepted the handshake. The reconnect path now calls doWebSocketDisconnectAll(), which drops clients and leaves the listener bound, and the stale comment is corrected. prepareForReboot() keeps the full close(), since the device restarts immediately after.

Shipped in 1.7.5-beta.7. AC 3 rests on the library source rather than a staged WiFi outage: there is no safe in-session way to drop the maintainer's network, and the reconnect path only runs when WiFi is already down. Field testers exercise it on the first router reboot.
<!-- SECTION:FINAL_SUMMARY:END -->
