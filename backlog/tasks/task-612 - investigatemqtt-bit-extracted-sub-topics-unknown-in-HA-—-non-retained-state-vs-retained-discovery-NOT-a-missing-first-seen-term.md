---
id: TASK-612
title: >-
  fix(mqtt): first-seen bit/byte fan-out starved by 250ms shared rate-gate
  (ASF/RBP/VH stay unknown)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-16 09:51'
updated_date: '2026-05-16 09:55'
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
Root cause (refined from the handover's hypothesis): the bit-extraction layer already HAS a first-seen term — all sub-topics (Status/StatusVH via publishStatusBitMQTT, ASF/RBP/VH/RemoteOverride via publishGatedBitMQTT) route through one shared gate shouldPublishTrackedStatusBit/Byte() which publishes on firstSeen || forcePublish || intervalElapsed, with a 60s heartbeat, RAM-only trackers reset on every ESP boot. The handover's "missing first-seen" / direction A/B premise does not hold.

The actual defect: that branch was throttled by a single GLOBAL mqttLastGatedPublishMs at 250ms (MQTT_GATED_PUBLISH_SPACING_MS), shared across every bit/byte slot. A parent OT message decodes its entire fan-out inside one processOT() call (ASF 1B+6b, RBP 2B+4b, Status 2B+15b) — microseconds apart — so only the first slot passed the spacing gate; the rest returned false (deferred), kept firstSeen=true, and retried only on the NEXT parent frame. msgId 0 Status recurs ~3s and its bits also change (valueChanged bypasses the gate) so it self-heals; ASF(5)/RBP(6)/VH parents are polled rarely, so their deferred first-seen publishes starved and those HA entities stayed "unknown" after a clean boot. Matches the field report and the maintainer's observation precisely.

Fix: in shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte(), rate-gate ONLY the recurring 60s heartbeat (intervalElapsed && !forcePublish); the one-shot first-seen and forcePublish bursts now bypass the 250ms spacing, exactly as valueChanged already does. Result: bits publish on first sight, then only on change or every 60s — the maintainer's stated semantics. First-seen is one-shot per slot per boot and parents arrive spread out, so the burst is bounded; the Status fan-out remains independently wrapped by begin/endStatusBurst cooldown for heap safety. TASK-402's spacing intent (spread the recurring fan-out) is preserved for the heartbeat path.

Maintainer direction: explicitly stated ("They should [publish on first seen] and after that only on change or every 60 seconds") — implemented as such.

Toutside 0x7FFF (cluster B2): unchanged; recommend document-only (filtering a sentinel is correct; HA "unknown" for boilers without an outside sensor is acceptable and should be noted in the recovery guide).

Files: src/OTGW-firmware/OTGW-Core.ino (both gate functions) + prerelease bump beta.6 -> beta.7. Evaluator unchanged 91.7% (only the pre-existing dev-baseline ADR-references failure, TASK-608).

Pending before leaving draft: firmware build (python build.py --firmware exit 0; not runnable in this sandbox — arduino-cli download network-blocked) and the field-test matrix (verify all bit sub-topics get >=1 publish within the first minute of OT traffic after each reboot path).
<!-- SECTION:FINAL_SUMMARY:END -->
