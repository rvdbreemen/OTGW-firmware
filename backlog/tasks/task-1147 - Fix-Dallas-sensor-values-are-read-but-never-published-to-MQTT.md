---
id: TASK-1147
title: 'Fix: Dallas sensor values are read but never published to MQTT'
status: To Do
assignee:
  - '@claude'
created_date: '2026-09-22 05:02'
updated_date: '2026-09-22 05:41'
labels:
  - bug
  - needs-info
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

CORRECTION to my earlier note, and it matters: mqttPublishAllowed is NOT the cause, and the sensor publish path is not permanently broken.

The instrumented build (+b317263) answered it directly. Over one 30 s run with the simulator:
- 280 GATEDIAG refusals by mqttPublishAllowed, of which ZERO carried a sensor address
- all 280 were the eight OT status bits (status_master, ch_enable, dhw_enable, cooling_enable, otc_active, ch2_enable, summerwintertime, dhw_blocking), 35 each, which is the interval gate doing exactly its designed job on unchanged values
- 6 sensor publishes SUCCEEDED: OTGW/value/otgw-84F3EB22B8E1/28D0000000000001 and the two siblings

So the earlier finding needs a qualifier. The two pre-instrumentation runs genuinely showed sensor reads with no sensor publish, twice, which is why I called it reproduced. But on a freshly flashed and rebooted device the same code publishes fine. The defect is therefore STATE-DEPENDENT, not a dead code path.

What differs between the failing and passing observations: the failing runs were on a device with roughly a day of uptime (firmware +b6b3c98), the passing run was minutes after a reboot. That points at something that latches over uptime rather than at a gate that is wrong by construction.

Note run 2 of the failing pair produced ZERO MQTT publishes of any kind over 35 s, not merely no sensor publishes. That reframes the symptom: it may never have been sensor-specific. The remaining silent gate is !MQTTclient.connected(), which device/info cannot disprove because mqttconnected there is a cached flag. That gate is now instrumented too, so the next occurrence will name itself.

Next step is to catch the failing state WITH the instrumentation in place, rather than to guess at the latch. The scaffolding must stay on the bench device until then.

RESOLVED, and the answer is that there is no bug in the Dallas MQTT path. My earlier reproduction was a measurement artifact and I am retracting it.

A local mosquitto was pointed at (single-field settings POST of mqttbroker only, so the stored MQTT password was never touched) and subscribed to # as a lossless observer. Three runs of the same simulator test, telnet observation next to broker observation:

  run1 | telnet: reads=6 pubs=0 | BROKER: 6 sensor messages | telnet undercounts by 6
  run2 | telnet: reads=9 pubs=9 | BROKER: 9 sensor messages | match
  run3 | telnet: reads=3 pubs=0 | BROKER: 6 sensor messages | telnet undercounts by 6

The broker received every sensor publish in all three runs. The telnet debug console dropped the corresponding log lines in two of them, and in run 3 it even dropped half the sensor READ lines. So what I measured earlier was the absence of a log line, not the absence of a publish.

Consequences:
1. The Dallas sensor to MQTT path is correct. sendMQTTData is called, the gates pass, the broker gets OTGW/value/<node id>/<16-hex address>.
2. indigo_lights report is NOT reproduced. It must not be answered as a confirmed firmware bug. The drafted Discord message saying it was confirmed was never sent, and must not be.
3. mqttPublishAllowed, the heap drop gate and the fragmentation gate were each independently exonerated earlier with counters, which still stands.

What IS real and worth keeping: the telnet console silently drops output under load, so any diagnosis resting on it alone is unreliable. That matches the known 2.0.0 note about telnet losing decode lines during publish bursts; this confirms the same on 1.x. A lossless broker subscription is the correct instrument for any publish question.

The diagnostic scaffolding has been reverted from the working tree. The bench still runs the instrumented build +b317263 and its broker setting has been restored to homeassistant.local.

Parked pending reporter input. Moved back to To Do and labelled needs-info, because the task as written has no valid premise any more: the Dallas MQTT path was shown to work, and the apparent failure was telnet log loss.

Do not start work on this from the title alone. Either it gets reframed around what indigo_light actually observes, or it is closed as not reproducible. The one genuine finding worth keeping, that the telnet console silently drops output under load, belongs in its own task rather than under this title.

Bench state: broker restored to homeassistant.local, diagnostic scaffolding reverted from the tree, and the device is being reflashed back to a build that matches committed code.
<!-- SECTION:NOTES:END -->
