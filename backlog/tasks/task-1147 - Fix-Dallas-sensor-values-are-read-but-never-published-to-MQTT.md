---
id: TASK-1147
title: >-
  feat: expose the PIC-attached temperature sensor (PR=E) over MQTT and HA
  discovery
status: To Do
assignee:
  - '@claude'
created_date: '2026-09-22 05:02'
updated_date: '2026-09-22 17:23'
labels:
  - enhancement
dependencies: []
references:
  - 'Discord #nederlandse-ondersteuning / indigo_light + .otgw / 2026-09-21'
priority: medium
ordinal: 226000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The OTGW hardware can carry a temperature sensor on the PIC. The PIC reports its reading through the PR=E command, and this firmware never asks for it, so the value reaches nothing: no MQTT topic, no Home Assistant entity, no REST field. Requested by indigo_light in Discord #nederlandse-ondersteuning on 2026-09-21, with the decisive detail supplied by .otgw (Schelte Bron): the sensor hangs on the PIC, not on a Wemos GPIO pin.

His use case is a boiler with no outdoor probe. He wants a wired sensor on the gateway to drive the outside temperature, and is currently working around it with a wireless Hue motion sensor fed through SAT.

EVIDENCE. His web interface OT log shows the value arriving:

    18:55:35.448290 > PR=E
    18:55:35.464041 < PR: E=19.19

The web interface renders the raw OT log, which is why the value is visible there while nothing carries it to MQTT.

VERIFIED IN CODE, three separate findings:

1. The firmware sends PR=A, B, C, D, G, I, L, M, N, O, P, Q, R, S and T. It never sends E, and there is no parser for a `PR: E=` response. The measured temperature therefore has no topic at all. This is the whole gap.

2. The existing "PIC Temp Sensor" entity is not a temperature and must not be mistaken for one. PR=D fills state.picSettings.sTempSensor (OTGW-Core.ino:796-798), published to otgw-pic/settings/temp_sensor and declared at mqtt_configuratie.cpp:1114 as msgid 250 sub 0x08 in the diagnostic category. It reports the sensor FUNCTION SETTING, so the 0 the reporter sees there is a configuration readout, not a failed measurement.

3. His "Outside Temperature" entity is OT MsgID 27 and stays empty because nothing on his bus supplies an outside temperature. That is precisely why he wants the PIC sensor.

SCOPE. Query PR=E on a sensible cadence, parse the reply, publish it, and give it Home Assistant discovery. Two design questions the implementer should settle with the maintainer before coding:

- What cadence. PR=E is a PIC round trip on the same serial line the OpenTherm traffic uses, so polling it too often costs bus time. The existing PIC settings block is fetched once; a live temperature needs repeating, which is a different pattern.
- Whether the value should merely be exposed as its own sensor, or should additionally be usable AS the outside temperature toward the boiler. The second is a larger decision: the firmware already accepts an outside override through <toptopic>/set/<node id>/outside, which translates to OT=<value>, so a Home Assistant automation can already close that loop today without any firmware change. Doing it inside the firmware would duplicate a path that already works.

Also worth deciding: whether a PIC without a sensor attached should publish nothing at all rather than a zero or an error string, following the same reasoning as the DHW water total, which stays silent until real data exists (ADR-094).

HISTORY. This record began as a bug report, "Dallas sensor values are read but never published to MQTT", and that framing was wrong twice over. The first investigation chased a Dallas sensor on a OneWire GPIO, which is a different subsystem entirely and works correctly. A second apparent reproduction turned out to be the test harness toggling MQTT debug off on alternate runs, since the debug keys are toggles rather than switches. Both retractions are in the notes below and are worth reading before trusting any earlier conclusion here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The firmware queries PR=E on an agreed cadence and parses the PR: E=<value> reply
- [ ] #2 The reading is published to its own MQTT topic with a Home Assistant discovery entry, typed as a temperature in degrees Celsius
- [ ] #3 A gateway with no sensor on the PIC publishes nothing rather than a zero or an error string, so an absent entity means no sensor and not a broken build
- [ ] #4 The existing PIC Temp Sensor diagnostic entity (PR=D, settings/temp_sensor) is left alone, and the new entity is named so the two cannot be confused
- [ ] #5 Polling PR=E does not measurably disturb OpenTherm traffic on the shared serial line
- [ ] #6 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [ ] #7 indigo_light confirms the value reaches Home Assistant on his gateway
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

The telnet loss that invalidated this task now has its own root cause and task: TASK-1148. SimpleTelnet write() discards the tail of a partial write and returns size anyway (SimpleTelnet_impl.tpp:218-238), so under burst the console silently truncates.

2026-09-22: the reporter answered the validation question, and .otgw (Schelte Bron) supplied the fact that reframes this whole task: "De sensor hangt aan de PIC, niet aan een Wemos pin." This was never about a Dallas sensor on a OneWire GPIO, which is the subsystem I investigated.

Evidence from indigo_light. His webGUI OT log shows:
  18:55:35.448290 > PR=E
  18:55:35.464041 < PR: E=19.19
So the value exists and reaches the web interface, because the web interface renders the raw OT log. He also reports the HA entity sensor.opentherm_gateway_otgw_outside_temperature carrying no value, and a "PIC Temp Sensor" entity reading 0. Firmware 1.7.5-beta.6+2bf8888, which is old.

Cause, verified in code:
1. The firmware sends PR=A,B,C,D,G,I,L,M,N,O,P,Q,R,S,T. It never sends E, and never parses a PR: E= response. The PIC-attached sensor temperature therefore has no MQTT topic at all.
2. The "PIC Temp Sensor" entity is not a temperature. PR=D fills state.picSettings.sTempSensor (OTGW-Core.ino:796-798), published to otgw-pic/settings/temp_sensor, declared at mqtt_configuratie.cpp:1114 as msgid 250 sub 0x08 in the diagnostic category. It reports the sensor FUNCTION SETTING, so a 0 there is a configuration readout, not a failed measurement.
3. His "Outside Temperature" entity is OT MsgID 27, which stays empty because nothing on his bus supplies an outside temperature. That is exactly why he wants the PIC sensor in the first place.

So this is a MISSING FEATURE, not a defect: expose the PIC-attached temperature sensor by querying PR=E and publishing the result. Nothing is broken in the sense the title claims.

The title and the bug label are now both wrong. Needs a maintainer decision: retitle and convert to a feature request, or close this and open a fresh one.
<!-- SECTION:NOTES:END -->
