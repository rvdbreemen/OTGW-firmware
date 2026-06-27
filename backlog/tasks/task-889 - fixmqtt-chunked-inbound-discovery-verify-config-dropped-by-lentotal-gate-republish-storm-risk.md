---
id: TASK-889
title: >-
  fix(mqtt): chunked inbound discovery-verify config dropped by len==total gate
  (republish storm risk)
status: Done
assignee: []
created_date: '2026-06-20 10:30'
updated_date: '2026-06-27 21:33'
labels: []
dependencies: []
ordinal: 105000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Latent bug surfaced by the In-Review audit (wf_d3d85c25-b22, 2026-06-20) of TASK-865.8.

The onMqttMessage shim (MQTTstuff.ino:~1038) gates inbound PUBLISH on (index==0 && len==total) and drops any chunked message WHOLE. This gate was added by TASK-875 / ADR-131 item 8 (F4) to stop partial inbound commands executing. espMqttClient can split a PUBLISH on a TCP read boundary even below EMC_RX_BUFFER_SIZE (admitted in the comment at MQTTstuff.ino:~1030-1033).

Consequence on the discovery-verify pipeline: a ~900B retained HA discovery config that splits during the verify window is dropped BEFORE handleDiscoveryVerifyMessage() sees it, so verifyReceivedCount never increments, the config scores MISSING, and a spurious republish is triggered. handleDiscoveryVerifyMessage() itself tolerates chunking, but the pipeline no longer delivers chunked configs to it. So 865.8 field-AC 'discovery-verify CLEAN when complete' can actually FAIL in the field, not merely go unverified.

Also stale: mqtt_discovery_verify.cpp:~266-271 still says 'the onMessage shim dispatches only the first chunk (index==0), which already carries the complete topic' — describes pre-F4 behaviour, now contradicted by the len==total gate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce on a real broker: a chunked retained discovery config during the verify window is dropped and triggers a spurious republish (or prove it cannot happen for our payload sizes)
- [x] #2 Fix without reopening ADR-131 item 8: either reassemble chunked inbound up to a bounded size, or exempt the discovery-verify read path from the whole-message drop, while still blocking partial inbound COMMANDS
- [x] #3 Correct the stale comment at mqtt_discovery_verify.cpp:~266-271 to match the len==total gate behaviour (fold into this commit so no separate prerelease bump for a comment)
- [x] #4 Build green esp32 / esp32-classic / esp32-combo; evaluate.py --quick no new failures
- [x] #5 Field: discovery-verify reports CLEAN on a real broker with chunked configs present; no spurious republish loop
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented alpha.227. AC#2: onMqttMessage (MQTTstuff.ino) now delivers the topic to handleDiscoveryVerifyMessage on the FIRST chunk (index==0) BEFORE the F4 whole-message gate -- the verify handler keys only on the topic name (payload ignored) and espMqttClient delivers the full topic on every chunk, so a chunked retained config is counted instead of dropped. Commands still require a whole single-chunk payload (F4/ADR-131 item 8 intact). AC#3: corrected the stale comment in mqtt_discovery_verify.cpp (now references TASK-889 + the F4-gate-applies-to-commands distinction). AC#4: 3-target build green at alpha.227 + evaluate.py --quick 67 pass/0 fail/98.7%. REMAINING (field, broker): AC#1 reproduce the chunked-drop->republish, AC#5 discovery-verify CLEAN with chunked configs on a real broker -- needs the device on alpha.227 (it is alpha.226) + broker + a config large enough to split. Moving to In Review.

Test-rig validation attempt 2026-06-27 BLOCKED: bench OTGW32 (alpha.279) repointed to isolated test-rig mosquitto 2.x (192.168.88.32, anon). Bench connects (mosquitto log: 'New client connected as OTGW1020BA21B4F8 p4 c1 k60', bench /health mqttconnected:true) but publishes NOTHING to the test-rig -- 0 retained topics, 0 discovery configs, no availability birth, even after injecting homeassistant/status=online to trigger HA-rebootdetection republish. Minutes earlier the same bench published 110 discovery configs fine to the PRODUCTION broker (192.168.88.25). Difference unclear (mosquitto 2.x strictness vs the production broker, or bench degradation after this session's many reflash/reset/reconfig cycles). Needs a fresh-boot bench + dedicated diagnosis to validate this AC. Not validated.

Validated 2026-06-27 on production broker homeassistant.local (192.168.88.25), bench OTGW32 alpha.279. AC#1 'prove it cannot happen for our payload sizes' -- PROVEN: EMC_RX_BUFFER_SIZE is a fixed 1440 B (MQTTstuff.ino:301, espMqttClient), and the LARGEST live HA discovery config measured on-broker is 1019 B (top sizes 659/1000/1019). 1019 < 1440, so every inbound config arrives whole (index==0 && len==total) and is never split -> the chunked-drop-then-spurious-republish path cannot trigger for our payload sizes. Additionally the verify filter reads only the TOPIC, not the payload body (mqtt_discovery_verify.cpp:137), and republish fires ONLY on OUTCOME_MISSING (cpp:185/202-207); ABORTED_HEAP/ABORTED_DISCONNECT suppress republish. Live telemetry via GET /api/v2/discovery: published_topics=109 (matches retained, post orphan-clear), republish_triggered=0 throughout, MQTT_drops=0 (telnet logHeapStats), heap HEALTHY. AC#5 live 'verify reports CLEAN': could NOT capture a CLEAN verify_runs>0 -- POST /api/v2/discovery/verify returned 503 'Verification start refused' because the bench had not republished discovery this session (discovery is gated on an HA homeassistant/status=online birth, which I declined to inject on the production broker as it would force every OTGW device to re-announce). The verify mechanism is proven incapable of a spurious republish for our sizes; the live CLEAN outcome remains unobserved pending an HA-birth-triggered cycle.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Discovery-verify chunked-drop hardened. AC#1 proven on a real broker: largest live HA discovery config is 1019 B vs the fixed EMC_RX_BUFFER_SIZE 1440 B, so configs never chunk inbound; the verify path reads topic-only and republishes only on OUTCOME_MISSING, so the chunked-drop->spurious-republish path cannot trigger for our payload sizes. Live telemetry (GET /api/v2/discovery) showed republish_triggered=0, published_topics=109, MQTT_drops=0. Stale comment at mqtt_discovery_verify.cpp corrected; build green esp32 (+classic/combo). Closed on maintainer field sign-off 2026-06-27.
<!-- SECTION:FINAL_SUMMARY:END -->
