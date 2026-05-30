---
id: TASK-769
title: >-
  Fix: MQTT malformed-packet disconnect from partial chunk write under low heap
  (ESP8266)
status: To Do
assignee: []
created_date: '2026-05-30 21:42'
labels:
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by GeorgeZ83 (geo83_44083) in Discord #english-support, 2026-05-29/30. ESP8266 OTGW fw 1.6.0+0542da1, MQTT to Home Assistant. Symptoms: HA sensors go unavailable, web UI freezes, broker drops the client with "malformed packet" then "session taken over".

Three-source evidence (cross-correlated):
1. Mosquitto broker log (George): client OTGWC8C9A35ACB08 disconnected: malformed packet at 23:45:23 and 23:51:21; session taken over at 00:01:34.
2. putty-telnet.log: matching MQTT outage, handleMQTT reconnect 23:52:03 after "offline 42044ms" — exactly the gap between the 23:51:21 malformed-packet drop and reconnect.
3. Heap critically low when web UI WebSocket live-log open: banner minFree 2312, logHeapStats dips to 5848 free / maxBlock 1296; WS churn ws_drops 15->26 from browser at .102.

Root cause (code-confirmed, MQTTstuff.ino): the streaming publish path beginMqttPublish()->writeMqttChunk()->endPublish() commits a fixed MQTT remaining-length in beginPublish(topic,len). writeMqttChunk (MQTTstuff.ino:283) bails when MQTTclient.write() short-writes (TCP sndbuf exhausted at low heap), but the caller (sendMQTTData MQTTstuff.ino:1004-1008, 1052-1056; sendMQTT 1230) then still calls MQTTclient.endPublish(). A header promising len bytes followed by fewer bytes desynchronises the MQTT TCP stream -> broker parses the next packet header as payload -> "malformed packet" -> disconnect. Reconnect with the same client-id while the old session lingers -> "session taken over". This matches the maintainer hypothesis (chunked streaming replaced the 1.2.0 buffered publish; pubsubclient unchanged).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 writeMqttChunk/writeMqttProgmemChunk short-write no longer leaves a partial MQTT packet on the wire (broker never sees malformed packet): on unrecoverable short-write the TCP connection is cleanly dropped (MQTTclient.stop) instead of calling endPublish() on a truncated payload
- [ ] #2 Add a bounded retry-with-yield on MQTTclient.write() short-writes so a started publish completes when lwIP sndbuf drains, rather than aborting on the first short write
- [ ] #3 Heap-guard threshold review: document the trade-off (lower guard = fewer drops but more partial-write disconnects) and only relax HEAP_LOW/HEAP_WARNING after the desync fix lands; record chosen values with rationale
- [ ] #4 python build.py --firmware exits 0
- [ ] #5 python evaluate.py --quick shows no new failures
- [ ] #6 Field validation by GeorgeZ83 on ESP8266 + HA: no malformed-packet/session-taken-over disconnects with web UI open
<!-- AC:END -->
