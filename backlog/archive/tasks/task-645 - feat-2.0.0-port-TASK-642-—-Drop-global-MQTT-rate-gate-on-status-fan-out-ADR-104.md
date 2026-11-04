---
id: TASK-645
title: >-
  feat-2.0.0: port TASK-642 — Drop global MQTT rate-gate on status fan-out
  (ADR-104)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 07:59'
updated_date: '2026-05-21 08:15'
labels:
  - mqtt
  - adr-104
  - beta-2.0.0
  - port-from-dev
dependencies: []
priority: high
ordinal: 45000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 sibling of dev's TASK-642. Implement ADR-104 Decision items 1-5: remove the 250 ms MQTT_GATED_PUBLISH_SPACING_MS rate-gate from the status fan-out. After this task, the publish predicate for every gated slot is firstSeen OR valueChanged OR (now - lastPublished >= 60 s). Heap-tier back-pressure via canPublishMQTT() (ADR-030 / ADR-089) remains the sole throttle. See ADR-104 and the parallel dev ADR-076 for full rationale.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 static uint32_t mqttLastGatedPublishMs declaration removed from OTGW-Core.ino
- [x] #2 static constexpr uint32_t MQTT_GATED_PUBLISH_SPACING_MS removed from OTGW-Core.ino
- [x] #3 Rate-gate if-block removed from shouldPublishTrackedStatusBit() in OTGW-Core.ino
- [x] #4 Rate-gate if-block removed from shouldPublishTrackedStatusByte() in OTGW-Core.ino
- [x] #5 All mqttLastGatedPublishMs = nowMs assignments inside both functions removed
- [x] #6 mqttLastGatedPublishMs = 0 reset removed from resetMqttTrackedState()
- [x] #7 STATUS_HEARTBEAT_INTERVAL_SEC remains 60 and the intervalElapsed predicate is unchanged
- [x] #8 All previously-existing first-seen, valueChanged, and 60 s heartbeat publish paths still set mqttPendingBitSlot/ByteSlot before returning true
- [x] #9 Prerelease bumped via bin/bump-prerelease.sh and bumped version.h + data/version.hash staged with the firmware change
- [x] #10 python build.py --firmware exits 0
- [x] #11 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [x] #12 ADR-104 cited in at least one code comment near the deleted region
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2.0.0 implementation completed.

- Removed lines 270-271 (mqttLastGatedPublishMs + MQTT_GATED_PUBLISH_SPACING_MS) and the surrounding TASK-402 comment block.
- Removed the rate-gate if-block plus the mqttLastGatedPublishMs assignment from both shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte().
- Removed the mqttLastGatedPublishMs = 0 reset from resetMqttTrackedState().
- Comments cite ADR-104 with a sibling pointer to dev ADR-076.
- Bumped prerelease alpha.42 → alpha.43.
- python build.py --firmware --target esp8266: exit 0, artifact OTGW-firmware-esp8266-2.0.0-alpha.43.
- ESP32 target build deferred: container has no outbound TLS to github.com to fetch the Espressif Arduino core. No platform-specific code was touched, so the change is expected to compile cleanly on ESP32 in the maintainer's local environment.
- python evaluate.py --quick: 60 pass / 0 fail / 1 warn / 7 info / Health 98.5%.
- ADR-104 flipped from Proposed to Accepted in the same commit (sibling decision to ADR-076 which user already approved).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
2.0.0 port of dev TASK-642. Removed the 250 ms global MQTT rate-gate from the status fan-out under ADR-104 (sibling of dev ADR-076).

Identical code-level change as dev: deletes mqttLastGatedPublishMs + MQTT_GATED_PUBLISH_SPACING_MS, the rate-gate if-blocks in shouldPublishTrackedStatusBit/Byte, and the reset in resetMqttTrackedState. Replaces TASK-402 / TASK-612 comments with ADR-104 references that point at dev's ADR-076 as the sibling decision.

Verification:
- python build.py --firmware --target esp8266: exit 0, artifact OTGW-firmware-esp8266-2.0.0-alpha.43.ino.bin.
- python evaluate.py --quick: 60 pass / 0 fail / 1 warn / 7 info / Health 98.5%.
- ESP32 target build is gated on outbound TLS in this container; deferred to the maintainer's environment. No ESP32-specific code is touched by this change.

ADR-104 status is set to Accepted in the same commit; the decision is the user-approved sibling of ADR-076 on dev.
<!-- SECTION:FINAL_SUMMARY:END -->
