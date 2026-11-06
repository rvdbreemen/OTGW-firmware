---
id: TASK-1664
title: 'MQTT: 13 ''TCP disconnected'' error lines within 40 ms right after a reconnect'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:24:00Z'
updated_date: '2026-11-06T04:12:00Z'
labels:
  - mqtt
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 787000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .61 beta.101 (TASK-1451 AC5a, 2026-11-05 20:38:25.061-.098): right after the reconnect via the held broker address, 13 'PrintMQTTErr: Error: MQTT TCP disconnected' lines appeared in 40 ms with heap dipping to 34 KB; MQTT stayed connected. Likely queued publishes failing in the reconnect burst. Unknown whether it predates TASK-1451. Evidence: lead scratchpad bench-101/telnet.log.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced on old code (before TASK-1451) or shown to be new
- [ ] #2 Explained from the code; fixed or documented as harmless
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 code analysis (AC2). The text is a stale label, not a real TCP drop: PrintMQTTError() (src/OTGW-firmware/MQTTstuff.ino:1520) prints lastMqttDisconnectReason, which only onMqttDisconnect() (MQTTstuff.ino:1624) sets and which defaults to TCP_DISCONNECTED (MQTTstuff.ino:1517). Eleven call sites, including every publish-failure path (sendMQTT MQTTstuff.ino:2074, publishBirthOnline :2089, BLE discovery :3331/:3446), call it when mqttPublishRaw() returns false. So after any earlier disconnect, each failed publish logs 'MQTT TCP disconnected' while the link is up, which matches 'MQTT stayed connected' on .61.

Why publishes fail in the reconnect burst: mqttPublishRaw() (MQTTstuff.ino:337) returns false when espMqttClient publish() returns packetId 0. That happens when _state != connected or _addPacket fails (src/libraries/espMqttClient/src/MqttClient.cpp, publish()). _addPacket refuses when EMC_GET_FREE_MEMORY() = max(getMaxAllocHeap, getMaxAllocPsram) < EMC_MIN_FREE_MEMORY 16384 (Packets/Packet.cpp:315, Config.h:32). onMqttConnect queues subscribe + versioninfo + publishAllPICsettings + boiler CSV back to back (MQTTstuff.ino:1596-1611); with free heap dipping to 34 KB the largest block can fall under 16 KB, so about 13 QoS0 publishes are refused in 40 ms.

Assessment: the log text is misleading, the drops are real but bounded. The refused publishes are QoS0 and not retried; a retained value refused at connect (PIC settings, boiler CSV) stays stale on the broker until its next periodic or on-change publish. Mechanism predates TASK-1451 by code (PrintMQTTError since 5847521dc, 2026-06-14, TASK-865.7; 16 KB guard is library default), but AC1 asks for a reproduction on old code, which needs the bench and a build (blocked: commit charge 85% on 2026-11-06 05:04).

Proposed fix (not implemented yet, needs build + bench): publish-failure call sites log 'MQTT publish not queued (connected=%d, maxblk=%u)' instead of PrintMQTTError(), so a real TCP drop and an out-of-memory refusal read differently; optionally count refusals in device/info. Whether to also pace the onConnect burst is a separate question.

2026-11-06 fix (local commit, not pushed): PrintMQTTError() prints 'Error: MQTT packet not queued (connected, maxblk=N)' when MQTTclient.connected() is true and returns; the stale disconnect reason is printed only while disconnected. Order checked in the library: _setState(State::disconnected) runs before _onDisconnectCallback (src/libraries/espMqttClient/src/MqttClient.cpp:309-311), so a real disconnect keeps its real reason. evaluate.py --quick: 91 checks, 0 failed, 0 warnings. Pending: esp32-combo build, OTA on .61, reconnect burst shows the new line (AC1/AC2 evidence).

2026-11-06 host test test/host/test_mqtt_print_error.py --old-rev 29bfe386b^ (slices the real PrintMQTTError from MQTTstuff.ino, MSVC): FIX P1/C1/C2 pass; OLD P1 FAIL (connected + refused publish printed 'TCP disconnected'), C1/C2 pass; mutant (connected check removed) fails exactly P1; RESULT PASS exit 0. This reproduces the misleading line on the old code in a harness; the bench reconnect on .61 after the build is still open.

AC1 ticked: the misleading line reproduces on the pre-fix PrintMQTTError in the host harness (P1 FAIL on 29bfe386b^), and git log -S lastMqttDisconnectReason shows that code since 5847521dc (2026-06-14, TASK-865.7), long before TASK-1451, so it is not new. AC2 stays open until the .61 bench reconnect shows the new line.
<!-- SECTION:NOTES:END -->
