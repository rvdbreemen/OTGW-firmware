---
id: TASK-889
title: >-
  fix(mqtt): chunked inbound discovery-verify config dropped by len==total gate
  (republish storm risk)
status: In Review
assignee: []
created_date: '2026-06-20 10:30'
updated_date: '2026-06-20 16:01'
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
- [x] #2 Fix without reopening ADR-131 item 8: either reassemble chunked inbound up to a bounded size, or exempt the discovery-verify read path from the whole-message drop, while still blocking partial inbound COMMANDS
- [x] #3 Correct the stale comment at mqtt_discovery_verify.cpp:~266-271 to match the len==total gate behaviour (fold into this commit so no separate prerelease bump for a comment)
- [x] #4 Build green esp32 / esp32-classic / esp32-combo; evaluate.py --quick no new failures
- [ ] #5 Field: discovery-verify reports CLEAN on a real broker with chunked configs present; no spurious republish loop
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented alpha.227. AC#2: onMqttMessage (MQTTstuff.ino) now delivers the topic to handleDiscoveryVerifyMessage on the FIRST chunk (index==0) BEFORE the F4 whole-message gate -- the verify handler keys only on the topic name (payload ignored) and espMqttClient delivers the full topic on every chunk, so a chunked retained config is counted instead of dropped. Commands still require a whole single-chunk payload (F4/ADR-131 item 8 intact). AC#3: corrected the stale comment in mqtt_discovery_verify.cpp (now references TASK-889 + the F4-gate-applies-to-commands distinction). AC#4: 3-target build green at alpha.227 + evaluate.py --quick 67 pass/0 fail/98.7%. REMAINING (field, broker): AC#1 reproduce the chunked-drop->republish, AC#5 discovery-verify CLEAN with chunked configs on a real broker -- needs the device on alpha.227 (it is alpha.226) + broker + a config large enough to split. Moving to In Review.
<!-- SECTION:NOTES:END -->
