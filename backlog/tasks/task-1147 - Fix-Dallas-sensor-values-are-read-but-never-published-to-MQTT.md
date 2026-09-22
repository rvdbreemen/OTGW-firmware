---
id: TASK-1147
title: 'Fix: Dallas sensor values are read but never published to MQTT'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-22 05:02'
updated_date: '2026-09-22 05:11'
labels:
  - bug
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning / indigo_light + .otgw / 2026-09-21'
priority: high
ordinal: 226000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported in Discord #nederlandse-ondersteuning by indigo_light, corroborated by .otgw (Schelte Bron): a sensor value shows in the OTGW web interface but is not published over MQTT, so it never reaches Home Assistant.

Reproduced on the bench (192.168.88.68, 1.7.6-beta.4+b6b3c98) using the built-in Dallas simulator, which runs the same publish path as real sensors because initSensors() sets bSensorsDetected = true in its simulation branch (sensors_ext.ino:145).

Observed over two runs via the telnet console with MQTT debug (key 3) and sensor debug (key 5) enabled, then sim on (key d):
- Sensors are read every poll: 'Sensor [0] 28D0000000000001 = 30.0C [sim]', three sensors, repeated each cycle.
- Not one MQTT publish carries a sensor address. In the first run 14 other publishes went out within 300 ms of the sensor poll (status_master, ch_enable, dhw_enable and so on), so MQTT itself was working.
- The REST endpoint /api/v2/sensors does report the values, which is why the web interface shows them. That is the asymmetry the reporter sees.

The heap gate is NOT the cause, which is worth recording because it was the first hypothesis: /api/v2/device/info reports hd_mqtt_drops 0 and hd_ws_drops 0, so canPublishMQTT() has never dropped a message on this device.

The publish call at sensors_ext.ino:286 is unconditional inside 'if (settings.mqtt.bEnable)', and that setting is true. The sensor debug line immediately above it prints, so execution reaches the call. sendMQTTData() must therefore be returning at one of its early gates, all of which sit above its own MQTTDebugTf logging line (MQTTstuff.ino:1038-1050). Remaining suspect is mqttPublishAllowed, the OTPublishGate interval gate, possibly observed closed because pollSensors() runs while an outer gated scope is open. Not yet proven.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A Dallas sensor value reaches MQTT under the simulator: a publish to <toptopic>/value/<node id>/<16-hex address> is observed on the telnet MQTT debug log
- [ ] #2 The exact gate that suppressed the publish is identified and named in the notes, not just worked around
- [ ] #3 The fix does not make sensor publishes bypass a heap or interval gate that exists for a reason
- [ ] #4 Verified on the bench with the simulator, and the reporter confirms real sensors reach HA
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Ruled out by reading the code, before instrumenting: the re-entrancy route. The OTPublishGate scope at OTGW-Core.ino:4511-4519 is tight, and decodeAndPublishOTValue() contains no feedWatchDog(), yield() or delay(). sendMQTTData() only reaches its own feedWatchDog() on the success path, so a CLOSED gate cannot yield into doBackgroundTasks() and re-enter pollSensors(). mqttPublishAllowed remains the suspect, but not via re-entrancy.

Also ruled out: the heap gate. /api/v2/device/info reports hd_mqtt_drops 0, so canPublishMQTT() has never refused on this device.

WHY INSTRUMENTATION IS IN THE TREE RIGHT NOW: two of the five gates in sendMQTTData() return false without logging anything, which is the reason this defect was invisible for so long. Added a temporary MQTTDebugTf on the mqttPublishAllowed and !connected branches of the char* overload only (MQTTstuff.ino:1041-1042), tagged GATEDIAG. This is diagnostic scaffolding, NOT the fix. It must be either removed or deliberately kept as a permanent diagnosability improvement before this task is committed. Do not ship it unreviewed.
<!-- SECTION:NOTES:END -->
