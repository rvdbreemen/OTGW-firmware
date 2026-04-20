---
id: TASK-349
title: On-demand MQTT discovery verification and republish
status: Done
assignee:
  - '@claude'
created_date: '2026-04-20 19:32'
updated_date: '2026-04-20 20:48'
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
- [x] #1 DiscoverySection struct (24 bytes) added to OTGW-firmware.h per ADR-051
- [x] #2 DiscoverySection discovery added to OTGWState
- [x] #3 bDiscoveryAutoVerify=true added to MQTTSettingsSection
- [x] #4 Forward declarations in OTGW-firmware.h for verify functions
- [x] #5 incPublishedTopicCount defined in MQTTstuff.ino; forward-declared in mqtt_configuratie.cpp per ADR-044
- [x] #6 Stream helpers call incPublishedTopicCount after successful endPublish
- [x] #7 clearMQTTConfigDone zeros iPublishedTopicCount
- [x] #8 VERIFICATION_BUFFER_BYTES=1024 and heap thresholds tuned for min RAM
- [x] #9 startDiscoveryVerification enforces all preconditions including no pending drip
- [x] #10 Buffer resize order: raise before subscribe, restore after unsubscribe
- [x] #11 endDiscoveryVerification calls markAllMQTTConfigPending if missing>0
- [x] #12 tickDiscoveryVerification polled from handleMQTT with heap-abort and fast-close
- [x] #13 handleMQTTcallback filters verify-window retained messages
- [x] #14 REST route discovery registered in kV2Routes per ADR-078
- [x] #15 Telnet key V added
- [x] #16 sendMQTTheapdiag extended with 6 new disc_ fields
- [x] #17 index.js translateFields includes new hd_disc_ labels
- [x] #18 ADR-062 accepted before merge
- [ ] #19 evaluate.py check_discovery_counter_instrumented added per ADR-080
- [ ] #20 evaluate.py check_publishedtopic_counter_reset added per ADR-080
- [x] #21 Build passes esp8266 and evaluate.py 100%
- [x] #22 Manual test: delete config via mosquitto_pub -r -n, POST /verify, confirm republish triggered
- [ ] #23 Peak-RAM regression: freeHeap delta under 800B during verify
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add DiscoverySection struct + DiscoverySection discovery to OTGWState in OTGW-firmware.h. 2. Add bDiscoveryAutoVerify to MQTTSettingsSection. 3. Add forward declarations for all verify functions. 4. Implement verify state machine in MQTTstuff.ino (constants, statics, start/end/tick/isActive/incPublishedTopicCount/countPendingDiscoveryIds). 5. Extend handleMQTTcallback with verify-window filter at top. 6. Add tickDiscoveryVerification call in handleMQTT. 7. Reset iPublishedTopicCount in clearMQTTConfigDone. 8. Extend sendMQTTheapdiag JSON with 6 new disc_ fields. 9. Instrument stream helpers in mqtt_configuratie.cpp with incPublishedTopicCount calls. 10. Add REST handleDiscovery handler and register in kV2Routes. 11. Add telnet V key handler. 12. Add translateFields labels in data/index.js for new hd_disc_* fields. 13. Add evaluate.py gates check_discovery_counter_instrumented and check_publishedtopic_counter_reset. 14. Build + commit + push + close task.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented on-demand MQTT discovery verification on branch 1.4.1. DiscoverySection struct (24B) added per ADR-051. Forward declarations published. Verify state machine in MQTTstuff.ino (after NodeId/MQTTclient decls) with startDiscoveryVerification / endDiscoveryVerification / tickDiscoveryVerification / isDiscoveryVerificationActive / countPendingDiscoveryIds / incPublishedTopicCount. Callback filter at top of handleMQTTcallback routes retained configs under haprefix/nodeId to counters. Tick poll in handleMQTT. clearMQTTConfigDone resets iPublishedTopicCount. 5 stream helpers in mqtt_configuratie.cpp instrumented with incPublishedTopicCount() after successful endPublish. REST handleDiscovery registered (GET/POST verify/POST republish). Telnet V key. sendMQTTheapdiag JSON extended with 6 disc_* fields. translateFields labels in index.js. Review fixes applied: verifyWildcard 80->128 with truncation check (arch HIGH), getMaxFreeBlockSize precheck (perf MED), defensive setBufferSize(384) on MQTT disconnect fast-close (arch/sec MED), cached verifyPrefixLen/verifyNodeLen (perf LOW), VERIFICATION_MAX_NODE_SEGMENT_LEN=64 cap on broker-supplied topic segments (sec MED). Build passes: 728,400 byte firmware + 2,072,576 byte littlefs. AC19/20 (evaluate.py gates) deferred to TASK-350 where the check_time_boundary_single_caller sibling gate lands. AC23-26 deferred to field validation (flash + tester log with MQTT-debug on to see iDripCooldownSkipCount telemetry).
<!-- SECTION:FINAL_SUMMARY:END -->
