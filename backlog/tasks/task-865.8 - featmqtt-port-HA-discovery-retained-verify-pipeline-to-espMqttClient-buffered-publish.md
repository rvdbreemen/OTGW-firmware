---
id: TASK-865.8
title: >-
  feat(mqtt): port HA discovery/retained/verify pipeline to espMqttClient
  buffered publish
status: To Do
assignee: []
created_date: '2026-06-13 05:50'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.7
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 2; HA discovery; depends seq7 = TASK-865.7)
Largest PubSubClient surface, split from the engine so they parallelize. MQTTHaDiscovery.cpp has ~20 stream*Discovery(PubSubClient&,...) using beginPublish/write/endPublish (dual-mode MqttJsonWriter MEASURE+WRITE 128B chunks). espMqttClient has NO streaming API -> buffered publish. The chunker is ESP8266-heap-only; worst-case discovery ~900B -> keep the MEASURE pass to size, allocate once, run WRITE into a RAM buffer, then one retain=true publish. Builds on seq7 mqttPublishRaw + connected().

## What
- MQTTHaDiscovery.cpp: every stream*Discovery (decls MQTTstuff.h:508-571; defs 2665/2709/2748/2883/2935/3126/3215/3304/3406/3506/3588/3684/3802/3846 + streamSatZone/PvBoost MQTTstuff.ino:2390-2485): drop the `PubSubClient &client` param -> mqttPublishRaw(topic,buf,byteCount,true); client.connected() guards -> MQTTclient.connected(); MqttJsonWriter MEASURE/WRITE (MQTTstuff.h:460-503) WRITE targets a RAM buffer not client.write.
- publishDiscoveryJson (MQTTstuff.ino:2298): same buffered conversion.
- verify TU mqtt_discovery_verify.cpp: drop verifyAccessorSetMqttBufferSize(1024) (:135/:143) - espMqttClient delivers oversize inbound CHUNKED via onMessage(index,total). Fix: match retained configs on TOPIC in handleDiscoveryVerifyMessage (treat any index as a hit, do not require full payload); remove the VERIFICATION_BUFFER_BYTES max-block preflight (97-110); keep VERIFICATION_MIN_HEAP_START.
- retained/availability to re-validate: ADR-006/040/041/052/062/066/073/077/093/100/102/106/116/118/124. QoS 0 for discovery (no outbox growth).

## Acceptance Criteria
- build: esp32 exit 0, all stream*Discovery converted (no PubSubClient param in MQTTHaDiscovery.cpp or MQTTstuff.h decls); zero beginPublish/endPublish/PubSubClient in MQTTHaDiscovery.cpp + mqtt_discovery_verify.cpp.
- build: verify TU no setBufferSize; handleDiscoveryVerifyMessage matches on topic + tolerates chunked inbound; payloads built into a MEASURE-sized transient buffer, single retain publish, buffer freed each call.
- evaluator: evaluate.py --quick no new failures (PROGMEM/String/heap-tier; ADR-077/088 pass or updated coherently).
- field: all HA entities appear after a fresh discovery (== PubSubClient baseline); retained configs survive a broker restart; discovery-verify (ADR-062) CLEAN when nothing missing, republishes when configs deleted.
- field: topology-migration (ADR-124) + legacy/modern toggle (ADR-106) still erase stale retained topics; no fragmentation regression from transient ~1KB/entity allocs on a long-uptime device.
<!-- SECTION:DESCRIPTION:END -->
