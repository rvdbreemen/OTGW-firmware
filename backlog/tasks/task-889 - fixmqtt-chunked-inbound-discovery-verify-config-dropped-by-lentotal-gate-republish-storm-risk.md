---
id: TASK-889
title: >-
  fix(mqtt): chunked inbound discovery-verify config dropped by len==total gate
  (republish storm risk)
status: To Do
assignee: []
created_date: '2026-06-20 10:30'
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
- [ ] #1 Reproduce on a real broker: a chunked retained discovery config during the verify window is dropped and triggers a spurious republish (or prove it cannot happen for our payload sizes)
- [ ] #2 Fix without reopening ADR-131 item 8: either reassemble chunked inbound up to a bounded size, or exempt the discovery-verify read path from the whole-message drop, while still blocking partial inbound COMMANDS
- [ ] #3 Correct the stale comment at mqtt_discovery_verify.cpp:~266-271 to match the len==total gate behaviour (fold into this commit so no separate prerelease bump for a comment)
- [ ] #4 Build green esp32 / esp32-classic / esp32-combo; evaluate.py --quick no new failures
- [ ] #5 Field: discovery-verify reports CLEAN on a real broker with chunked configs present; no spurious republish loop
<!-- AC:END -->
