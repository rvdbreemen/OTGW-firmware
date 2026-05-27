---
id: TASK-402
title: >-
  Rate-gate MQTT gated fanout publishes at >=1s spacing with per-slot pending
  flags
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 12:26'
updated_date: '2026-04-24 12:39'
labels:
  - mqtt
  - heap
  - fanout
  - home-assistant
  - optimization
  - rate-limit
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Prevent heartbeat and first-seen bursts by enforcing at least 1 second between any two gated bit/byte publishes for first-seen / heartbeat / force paths. Change-detect publishes bypass the spacing gate entirely and publish immediately (absolute priority per user spec: 'change detectie moet altijd zo snel mogelijk gepubliceerd worden'); multiple bit-flips within 1s are accepted. Covers all 5 gated fanout arrays (msgId 0 Status, msgId 70 Status VH, msgId 5 ASF, msgId 6 RBP, msgId 100 Remote Override). No per-slot pending flags needed because firstSeen and intervalElapsed sentinels naturally retry on subsequent OT frames until the gate passes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Add global mqttLastGatedPublishMs (uint32_t, millis()-based) + MQTT_GATED_PUBLISH_SPACING_MS constant (1000ms)
- [x] #2 shouldPublishTrackedStatusBit: change-detect (valueChanged && !firstSeen) publishes immediately with no spacing check and does NOT update mqttLastGatedPublishMs
- [x] #3 shouldPublishTrackedStatusBit: firstSeen / forcePublish / intervalElapsed publishes apply spacing gate; if millis()-mqttLastGatedPublishMs < 1000ms, return false (defer, retries naturally on next frame); otherwise publish and update mqttLastGatedPublishMs
- [x] #4 shouldPublishTrackedStatusByte: identical change-detect-bypass + rate-gate logic
- [x] #5 mqttLastGatedPublishMs sentinel 0 (boot / post-reset) treated as 'never published yet — first publish is free'
- [x] #6 resetMqttTrackedState resets mqttLastGatedPublishMs to 0
- [x] #7 Build clean on ESP8266 (python build.py --firmware): no warnings, no errors
- [x] #8 python evaluate.py --quick shows 0 new failures vs post-TASK-401 dev baseline
- [ ] #9 Runtime verification (user-performed on hardware): MQTT subscriber confirms boot-time fanout spreads over roughly 40-50 seconds at >=1 publish/second; at 60s heartbeat time the 16-bit msgId 0 storm spreads over ~16 seconds instead of firing in one frame; bit-flips during normal operation publish within the same OT frame (no artificial delay)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add file-scope globals: mqttLastGatedPublishMs (uint32_t) + MQTT_GATED_PUBLISH_SPACING_MS (1000).\n2. Add 10 pending-flag arrays at file scope.\n3. Extend shouldPublishTrackedStatusBit signature with uint16_t *pendingBits parameter; implement spacing gate + pending management.\n4. Extend shouldPublishTrackedStatusByte signature with uint8_t *pendingBytes parameter; same logic.\n5. Update shouldPublishStatusBit / shouldPublishStatusVHBit / shouldPublishStatusByte / shouldPublishStatusVHByte to pass pending array.\n6. Update publishGatedBitMQTT / publishGatedByteMQTT (both FlashStringHelper and char* overloads) to take pending pointer and pass through.\n7. Update all call sites in print_ASFflags, publishRBPFlagsState, print_remoteoverridefunction to pass their pending arrays.\n8. Add pending-flag resets to resetMqttTrackedState().\n9. Build + evaluate.\n10. Commit + push.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-402 implements the 1-second rate-gate the user requested: between any two non-change gated publishes there is now >=1000ms spacing, actively tracked via a single global millis()-based timer. Change-detect publishes bypass the gate entirely and fire immediately per user spec ('change detectie moet altijd zo snel mogelijk gepubliceerd worden') — multiple bit-flips within 1s are accepted.

Changes (OTGW-Core.ino):
- Added global mqttLastGatedPublishMs (uint32_t) + MQTT_GATED_PUBLISH_SPACING_MS (1000) near the other MQTT tracker globals. Sentinel value 0 means 'never published, first publish free'.
- shouldPublishTrackedStatusBit: restructured into two-phase decision. Phase 1: if valueChanged (and !firstSeen), publish immediately without consulting the spacing gate and WITHOUT updating mqttLastGatedPublishMs. Phase 2: if firstSeen/forcePublish/intervalElapsed, consult spacing gate — defer if (millis() - mqttLastGatedPublishMs) < 1000ms (returns false, lastTime unchanged, bit keeps retrying via firstSeen/intervalElapsed sentinel on subsequent frames); otherwise publish and update mqttLastGatedPublishMs.
- shouldPublishTrackedStatusByte: same two-phase structure.
- resetMqttTrackedState resets mqttLastGatedPublishMs to 0 so post-reset first publish bypasses spacing (avoid a pointless 1s wait).
- No per-slot pending flags needed: firstSeen + intervalElapsed + forcePublish sentinels naturally retry via their existing storage (lastTime stays unchanged on deferral, so the same flag remains true on the next OT frame). This keeps the change surface tiny (+40 / -6 lines) and reuses all existing infrastructure.

Impact (verified against log of TASK-400 heartbeat event at 14:33:41-43 on hardware):
- msgId 0 heartbeat burst: 16 bit + 2 byte publishes previously fired within a ~3 second window with peak dHeap=-3360 bytes. Now spread across ~18 OT frames at 1 publish/frame (~18 seconds for msgId 0, still well within a 60s window). Expected peak dHeap per frame: ~-400 bytes.
- handleMQTT peak: previously 16268us during heartbeat. Now expected ~2000us per frame distributed over 16 frames.
- HA reconnect recovery: full boot-time fanout now takes ~44 seconds (44 gated publishes at 1s spacing) but is acceptable — change-detect is still instant.
- Change-detect latency: UNCHANGED (immediate, bypasses gate). Real fault-bit flips go out within the OT frame they occur.

Scope: covers all 5 gated fan-out arrays (msgId 0 Status, msgId 70 Status VH, msgId 5 ASF, msgId 6 RBP, msgId 100 Remote Override) because they all route through shouldPublishTrackedStatusBit/Byte.

Verification:
- python build.py --firmware -> exit 0, 0 warnings, 0 errors.
- python evaluate.py --quick -> 31/31 passed, 2 pre-existing warnings, 94.3% health.
- Runtime verification left to user on hardware (AC #8): flash, wait >60s, observe MQTT topic burst distribution + dHeap reduction during heartbeat events.
<!-- SECTION:FINAL_SUMMARY:END -->
