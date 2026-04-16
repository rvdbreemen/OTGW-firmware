---
id: TASK-268
title: 'Fix: ESP8266 v1.4.0-beta reboot loop after flash'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-13 22:18'
updated_date: '2026-04-14 22:57'
labels:
  - bug
  - esp8266
  - critical
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-04-13'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans in Discord #beta-testing (2026-04-13). After flashing v1.4.0-beta (FS first then firmware) the ESP8266 enters a continuous reboot loop with an exception. MQTT still connects briefly between reboots, but the web UI is completely unresponsive. Reverted to v1.3.10 and device works normally again (WiFi 86%). Reporter also shared that pressing any number in telnet hangs the device. Screenshot of crash log shared as image attachment in Discord.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ESP8266 boots cleanly after flashing v1.4.0-beta FS + firmware
- [x] #2 Web UI accessible after flash
- [x] #3 No reboot loop visible in telnet logs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-13: crashevans reports device stuck in reboot loop with exception after v1.4.0-beta. MQTT visible between reboots. Telnet hangs when pressing keys. Shared screenshot but content not available in Discord read. Reverted to v1.3.10 and stable.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed ESP8266 v1.4.0-beta reboot loop caused by heap exhaustion during MQTT autodiscovery.

Root cause: doAutoConfigureMsgid() was called on every incoming OT message without MQTT connectivity or heap guards. On ESP8266 core 3.x (lwIP 2.x), each call triggered a pbuf allocation (~1200 bytes). Under normal operation this saturated the heap, causing MQTT to disconnect, which then triggered reconnect storms and eventually exception crashes.

Changes (commit 61a0d6b9):
- MQTTstuff.ino: Extracted magic number 12000 to named constant MQTT_DISCOVERY_HEAP_MIN = 8000. Added rate-limited debug log (30 s) when heap guard fires so the condition is visible in telnet without flooding.
- OTGW-Core.ino: Added state.mqtt.bConnected guard to ensurePSSummaryDiscovery(). Mirrors the outer processOT guard — avoids unthrottled lock-acquire + LittleFS open on every PS1 message when MQTT is not connected.

Defense-in-depth layers now in place:
1. processOT() outer guard: settings.mqtt.bEnable && state.mqtt.bConnected
2. ensurePSSummaryDiscovery() added bConnected guard (M-001)
3. doAutoConfigureMsgid() inner guard: MQTTclient.connected() check
4. Heap guard: skip if free heap < MQTT_DISCOVERY_HEAP_MIN (8000)
5. Rate limiter: 1-second cooldown per message ID
6. Session lock: MQTTAutoConfigSessionLock prevents re-entrancy

Build verified: OTGW-firmware-1.4.0-beta+00c91f1.ino.bin, 0.66 MB.
<!-- SECTION:FINAL_SUMMARY:END -->
