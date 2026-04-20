---
id: TASK-349
title: On-demand MQTT discovery verification and republish
status: To Do
assignee: []
created_date: '2026-04-20 19:32'
labels:
  - mqtt
  - discovery
  - observability
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a verification pass that subscribes to the node-scoped discovery wildcard haprefix/+/nodeId/#, counts retained configs delivered by the broker, compares to the expected count, and re-announces via markAllMQTTConfigPending when counts do not match. RAM-tuned: PubSubClient RX buffer raised 384 to 1024 during 15s window (peak +640B). Exposed via REST GET/POST /api/v2/discovery, telnet V key, and hourly heapdiag. See plan file expressive-growing-yao and ADR-062.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 DiscoverySection struct (24 bytes) added to OTGW-firmware.h per ADR-051
- [ ] #2 DiscoverySection discovery added to OTGWState
- [ ] #3 bDiscoveryAutoVerify=true added to MQTTSettingsSection
- [ ] #4 Forward declarations in OTGW-firmware.h for verify functions
- [ ] #5 incPublishedTopicCount defined in MQTTstuff.ino; forward-declared in mqtt_configuratie.cpp per ADR-044
- [ ] #6 Stream helpers call incPublishedTopicCount after successful endPublish
- [ ] #7 clearMQTTConfigDone zeros iPublishedTopicCount
- [ ] #8 VERIFICATION_BUFFER_BYTES=1024 and heap thresholds tuned for min RAM
- [ ] #9 startDiscoveryVerification enforces all preconditions including no pending drip
- [ ] #10 Buffer resize order: raise before subscribe, restore after unsubscribe
- [ ] #11 endDiscoveryVerification calls markAllMQTTConfigPending if missing>0
- [ ] #12 tickDiscoveryVerification polled from handleMQTT with heap-abort and fast-close
- [ ] #13 handleMQTTcallback filters verify-window retained messages
- [ ] #14 REST route discovery registered in kV2Routes per ADR-078
- [ ] #15 Telnet key V added
- [ ] #16 sendMQTTheapdiag extended with 6 new disc_ fields
- [ ] #17 index.js translateFields includes new hd_disc_ labels
- [ ] #18 ADR-062 accepted before merge
- [ ] #19 evaluate.py check_discovery_counter_instrumented added per ADR-080
- [ ] #20 evaluate.py check_publishedtopic_counter_reset added per ADR-080
- [ ] #21 Build passes esp8266 and evaluate.py 100%
- [ ] #22 Manual test: delete config via mosquitto_pub -r -n, POST /verify, confirm republish triggered
- [ ] #23 Peak-RAM regression: freeHeap delta under 800B during verify
<!-- AC:END -->
