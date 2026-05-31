---
id: TASK-769
title: >-
  Fix: MQTT malformed-packet disconnect from partial chunk write under low heap
  (ESP8266)
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-30 21:42'
updated_date: '2026-05-31 09:16'
labels:
  - bug
dependencies: []
references:
  - >-
    Discord #english-support / GeorgeZ83 (geo83_44083) / 2026-05-29..30 / logs:
    putty-telnet.log + log.txt + mosquitto broker log
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
- [ ] #7 Discovery composer path (mqtt_configuratie.cpp stream*Discovery, 7 sites): failure branch must drop TCP via client.disconnect() instead of client.endPublish() on a truncated payload — this is the largest-payload path, most prone to short-write desync under heap pressure
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. writeMqttChunk + writeMqttProgmemChunk (MQTTstuff.ino:283,300): add bounded retry-with-yield() on short MQTTclient.write() so a started publish completes when lwIP sndbuf drains.
2. On genuine failure (retries exhausted): callers (sendMQTTData 1004-1008, PROGMEM overload 1052-1056, sendMQTT 1230) must drop the TCP link via MQTTclient.stop() instead of endPublish() on a truncated payload — prevents malformed packet to broker.
3. Add MQTT_WRITE_MAX_RETRIES constant.
4. python build.py (full) exit 0; python evaluate.py --quick green.
5. Commit MQTTstuff.ino only (leave unrelated dirty UI files). Do NOT push — field validation by George pending.
6. Guard-relax (HEAP_LOW/WARNING) deferred to follow-up after George validates desync fix.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related prior work: TASK-242 (serial overrun + MQTT throttle drops, Done).

Heap tiers at time of report (helperStuff.ino:900-902): CRITICAL=1536 (block), WARNING=3072 (throttle 500ms), LOW=5120 (throttle 100ms); frag-promote when freeHeap in LOW band and maxBlock<1536. Throttle MS: helperStuff.ino:915-918.

User request under evaluation: lower heap-guard thresholds to reduce MQTT drops. RISK: relaxing the guard BEFORE the desync fix makes publishes proceed at lower heap -> more short-writes -> MORE malformed-packet disconnects. Correct order = fix writeMqttChunk desync first, then relax guard.

Desync fix implemented + committed 7e5a61ed (MQTTstuff.ino only).
- writeMqttChunk/writeMqttProgmemChunk: bounded retry-with-yield (MQTT_WRITE_MAX_RETRIES=10) on short MQTTclient.write().
- 3 caller failure branches: MQTTclient.disconnect() instead of endPublish() on truncated payload (agent first used non-existent PubSubClient.stop(); corrected to disconnect()).
- Build: python build.py exit 0. Evaluate --quick: 34/34 pass, 0 fail, 100%.
- NOT pushed (field validation by George pending). Guard-relax deferred.

Discovery-path gap found + fixed (2nd commit f8314a0b, mqtt_configuratie.cpp +7/-7). 7 stream*Discovery composers (sensor/binsensor/number/climate-ish/button/select) called client.endPublish() on writer.ok==false (truncated) -> same desync. Now client.disconnect(). This is the largest-payload path, most likely to short-write under George heap pressure. Build exit 0, eval 100%, adr-judge 0 violations. Dev TASK-769 commits: 7e5a61ed + f8314a0b. Not pushed (George field validation pending).
<!-- SECTION:NOTES:END -->
