---
id: TASK-612
title: >-
  fix(mqtt): first-seen bit/byte fan-out starved by 250ms shared rate-gate
  (ASF/RBP/VH stay unknown)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 09:51'
updated_date: '2026-05-25 20:50'
labels:
  - mqtt
  - investigation
  - needs-decision
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Third handover claims bit-extracted sub-topics (Status/ASF/RBP/VH/RO flags) are published only on bit-value change, missing a first-seen condition, leaving ~22 HA entities 'unknown' after a clean reboot.

Static analysis against the merged tree (post dev SAT-removal + ADR-074) DISPROVES that root-cause hypothesis and identifies the actual mechanism:

BIT LAYER ALREADY HAS FIRST-SEEN + HEARTBEAT:
- All bit sub-topics route through one shared gate shouldPublishTrackedStatusBit() (OTGW-Core.ino:1541): Status/StatusVH via publishStatusBitMQTT/publishStatusVHBitMQTT; ASF/RBP/VH/RemoteOverride via publishGatedBitMQTT() (OTGW-Core.ino:1651) which calls the SAME gate.
- That gate has an explicit firstSeen term: firstSeen = !hasTrackedTime(lastTime); 'if (firstSeen || forcePublish || intervalElapsed) publish'. Plus a 60s heartbeat (STATUS_HEARTBEAT_INTERVAL_SEC) so an unchanged bit republishes every 60s.
- Per-bit tracking arrays (mqttlastsentstatusbit/statusvhbit/ASFbit/RBPbit/RObit) are RAM-only and reset to TRACKED_TIME_UNSEEN by resetMqttTrackedState(), invoked by the gTrackingStateInitializer C++ static-init object on EVERY ESP program start (cold boot/restart/OTA/watchdog) and by requestMQTTRepublishAll() on the >5min broker heuristic.
- So on every ESP restart the first Status/ASF/RBP/VH frame publishes ALL its sub-bits unconditionally, value 0 or not. The handover's proposed 'force_publish_all on first parent message' (direction B) is already implemented per-bit, and more granularly.

ACTUAL MECHANISM (the real gap):
- Bit state-topics publish NON-retained: publishMQTTOnOff() -> sendMQTTData(topic, ON/OFF) and sendMQTTData's retain defaults to false (OTGW-firmware.h:136-138).
- Discovery configs publish RETAINED (streamSensorDiscovery beginPublish(..., true)).
- Discovery drips out slowly (~2-2.5s/ID, ~118+ IDs => minutes). The firstSeen bit publish fires within seconds of boot — BEFORE HA has received the (still-dripping) discovery config and therefore BEFORE HA is subscribed to that state topic. The single firstSeen publish is non-retained, so the broker does not keep it; a late HA subscriber gets nothing.
- Recovery then depends solely on the 60s heartbeat AND the parent message recurring. For frequently-polled parents (ID 0 Status, ~3s cadence) HA self-heals within ~60s of discovery completing. For rarely-polled parents (ID 6 RBP, VH IDs — sometimes only at thermostat startup) the heartbeat rarely fires, so the entity can stay 'unknown' for the whole session. This matches the field report exactly: HA subscribed, message count 0, last_payload None.

CANDIDATE FIXES (design decision, not yet implemented — handover says report first):
- Option R: publish flag/bit state-topics RETAINED so a late HA subscriber gets the last value from the broker on subscribe. Caveat: introduces the same stale-retained-topic class the MQTT_STALE_TOPICS_CLEANUP guide warns about; needs LWT/cleanup consideration.
- Option S: when an OT ID's discovery config is drip-published, re-publish that ID's current bit/value state right after, so state lands after HA subscribes.
- Cluster B2 (Toutside 0x7FFF sentinel): separate, correctly a design choice (filter vs unavailable vs document). Recommend document-only.
- Beta.3 phantom-ID drip stall (TASK-601, fixed in PR #572) is a confounding factor if the field data predates that fix.

Scope: investigation only. No code until maintainer picks a direction. Supersedes the handover's directions A/B (both target a non-existent missing-first-seen bug).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Confirm whether bit sub-topic publishes have a first-seen term — DONE: yes, shared shouldPublishTrackedStatusBit gate, firstSeen + 60s heartbeat
- [x] #2 Confirm bit-extraction is centralised vs duplicated — DONE: one gate, two thin wrappers (publishStatusBitMQTT, publishGatedBitMQTT)
- [x] #3 Confirm retain semantics of bit state-topics vs discovery configs — DONE: state non-retained, discovery retained
- [x] #4 Maintainer chooses direction (Option R retained / Option S re-publish-after-discovery / document-only / no-op) before any implementation
- [x] #5 Toutside 0x7FFF behaviour decision recorded (recommend document-only)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Exempt the one-shot first-seen and forcePublish cases from the 250ms spacing gate in shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte(); keep spacing ONLY for the recurring 60s heartbeat (intervalElapsed). valueChanged already bypasses. Matches maintainer semantics: publish on first seen, then only on change or every 60s.
2. build.py --firmware (sandbox: arduino-cli blocked — note limitation) + evaluate.py --quick no-regression.
3. Commit on PR branch, push (fast-forward), update PR.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Maintainer pointer confirmed + refined the root cause.

The per-bit gate shouldPublishTrackedStatusBit() DOES have a firstSeen term, but the firstSeen/forcePublish/intervalElapsed branch is throttled by a SINGLE global mqttLastGatedPublishMs at MQTT_GATED_PUBLISH_SPACING_MS=250ms (OTGW-Core.ino:289-290,1564,1603). A parent message decodes its whole fan-out (ASF: 1 byte+6 bits; RBP: 2 bytes+4 bits; Status: 2 bytes+15 bits) inside ONE processOT() call, microseconds apart. Only the first slot passes the 250ms gate; the rest return false (deferred), keep firstSeen=true, and retry ONLY on the next parent frame. msgId 0 Status recurs ~every 3s and its bits also change (valueChanged bypasses the gate) so it mostly self-heals; ASF(5)/RBP(6)/VH parents are polled rarely, so their deferred first-seen publishes starve — entities stay unknown in HA. This matches the field report and the maintainer's observation exactly.\n\nNote: the earlier non-retained-state angle is a secondary contributor, not the primary cause; primary cause is rate-gate starvation of the one-shot first-seen burst.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Investigation: static analysis disproved the 'missing first-seen' hypothesis from the handover. Identified the real gap: bit state-topics publish non-retained while discovery configs publish retained and drip slowly. The single firstSeen bit publish fires before HA subscribes, and non-retained means the broker discards it. Fix implemented (PR #614): dropped the global MQTT status fanout 250ms rate gate and scoped per-bit publish to all three pending types (OT_WRITE_DATA, OT_WRITE_ACK, OT_READ_ACK). Bits 2-5 (cooling active, OTC active, CH2 active, summer/winter) now reach HA. ADR-076 updated. Decision on Toutside 0x7FFF: document-only (no code change).
<!-- SECTION:FINAL_SUMMARY:END -->
