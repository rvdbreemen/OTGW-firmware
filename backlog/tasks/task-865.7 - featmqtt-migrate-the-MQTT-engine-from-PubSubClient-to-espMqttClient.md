---
id: TASK-865.7
title: 'feat(mqtt): migrate the MQTT engine from PubSubClient to espMqttClient'
status: To Do
assignee: []
created_date: '2026-06-13 05:49'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.4
  - TASK-865.6
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 2; engine; depends seq4=TASK-865.4, seq6=TASK-865.6)
Replace PubSubClient with non-blocking espMqttClient (bertmelis), the client EMS-ESP32 uses (see other-projects/EMS-ESP32-dev). PubSubClient::connect() is synchronous (capped setSocketTimeout(5), accepted in ADR-108) and stalls the loop; espMqttClient::connect() is async -> that whole sync-blocker class (and ADR-108) dissolves. The lib_dep is added by seq4 (do NOT re-declare). Owns the engine: client instance, handleMQTT() state machine, LWT/birth, inbound callback, subscribe, the publish primitive seq8 builds on.

## What (MQTTstuff.ino unless noted)
- :222 PubSubClient MQTTclient(wifiClient) -> espMqttClient MQTTclient; drop include PubSubClient.h (:13, MQTTstuff.h:14); drop PubSubClient from lib_deps (platformio.ini:28).
- handleMQTT() 1061-1281: connect() async -> MQTT_STATE_TRY_TO_CONNECT no longer blocks. Keep thin retry/backoff (timerMQTTwaitforconnect 1214-1220); move post-connect work (birth/subscribe/republish/versioninfo 1150-1199) into onConnect(); disconnect + state.mqtt.bConnected into onDisconnect().
- remove MQTTclient.setSocketTimeout(5) (:1103); setKeepAlive(60) maps over.
- setServer/setClientId/setCleanSession(true); setCallback->onMessage().
- LWT: setWill(MQTTPubNamespace,0,true,(const uint8_t*)"offline",7) BEFORE connect(). espMqttClient STORES pointers (not copies): MQTTPubNamespace is file-static (:231) + literal -> safe; document the pointer-lifetime contract. setCredentials() before connect. Birth (retained online) in onConnect.
- handleMQTTcallback (:831) -> onMessage(props,topic,payload,len,index,total): inbound is tiny (index==0 && len==total), thin shim asserting it; keep copyMQTTPayloadToBuffer guard (:859).
- keep MQTTclient.loop() (1074,1227); document who pumps it if MQTT later moves to a task.
- Publish primitive (the seq8 interface): replace sendMQTTData (:1312)/sendMQTT (:1678)/chunk-writer trio (:322-382). The 128B chunker is ESP8266-heap-only (comments 20-24,1676-1677) -> single buffered publish into a transient heap buffer (~900B worst-case discovery, trivial on S3). Declare `bool mqttPublishRaw(const char* topic, const uint8_t* payload, size_t len, bool retain)` in MQTTstuff.h as the single chokepoint. Remove the disconnect-on-truncated-write desync guard (TASK-770: 1332/1380/1686/3132) - espMqttClient frames atomically.
- state.mqtt.bConnected (:1280) -> connected().
- ADR-108: write a superseding note under ADR-123 (5s-sync-blocker premise gone; the forbid_pattern gate forbids CHANGING setSocketTimeout(5), not removing it).

## Acceptance Criteria
- build: esp32 exit 0, espMqttClient linked, PubSubClient removed (grep per-env SUCCESS); zero PubSubClient/setSocketTimeout/setCallback/MQTTclient.connect( in src .ino/.h.
- build: mqttPublishRaw declared in MQTTstuff.h, single publish chokepoint; LWT via setWill(...) before connect() with the pointer-lifetime comment.
- build: birth + republish/subscribe/versioninfo in onConnect; bConnected from onConnect/onDisconnect.
- evaluator: evaluate.py --quick no new failures; ADR-108 forbid_pattern not tripped by line removal; ADR-108 superseded/annotated; ADR consistency green.
- field: broker kill -> WebUI/graph stays responsive (no 5s stall); HA avty_t online(birth)/offline(LWT within keepalive); set/<nodeId>/<cmd> still reaches the PIC/OTDirect queue.
<!-- SECTION:DESCRIPTION:END -->
