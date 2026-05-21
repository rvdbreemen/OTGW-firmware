---
id: TASK-642
title: Drop global MQTT rate-gate on status fan-out (ADR-076 Decision items 1-5)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 07:36'
updated_date: '2026-05-21 07:41'
labels:
  - mqtt
  - adr-076
  - task-402-fixup
  - beta
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement ADR-076 Decision items 1-5: remove the 250 ms MQTT_GATED_PUBLISH_SPACING_MS rate-gate from the status fan-out. The global rate-gate (mqttLastGatedPublishMs token) caused per-bit slot starvation on heartbeat ticks because all sibling slots in a single processOT() fan-out simultaneously hit intervalElapsed=true; the byte slot grabbed the token first and the bit slots lost it for the next 60 s heartbeat too. Bits 2-5 of OT_Statusflags consistently entered HA as unknown as a direct result. After this task, the publish predicate for every gated slot is firstSeen OR valueChanged OR (now - lastPublished >= 60 s) — exactly ADR-052's per-slot contract with no global override. Heap-tier back-pressure via canPublishMQTT() (ADR-030) remains the sole throttle.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Static declaration of mqttLastGatedPublishMs is removed from OTGW-Core.ino (was line 289)
- [x] #2 Constexpr MQTT_GATED_PUBLISH_SPACING_MS is removed from OTGW-Core.ino (was line 290)
- [x] #3 Rate-gate if-block (intervalElapsed && !forcePublish && mqttLastGatedPublishMs != 0 && (nowMs - mqttLastGatedPublishMs) < MQTT_GATED_PUBLISH_SPACING_MS) is removed from shouldPublishTrackedStatusBit() in OTGW-Core.ino
- [x] #4 Rate-gate if-block is removed from shouldPublishTrackedStatusByte() in OTGW-Core.ino
- [x] #5 mqttLastGatedPublishMs = nowMs assignments inside both functions are removed (no remaining writes to the deleted global)
- [x] #6 mqttLastGatedPublishMs = 0 reset is removed from resetMqttTrackedState() (was line 437)
- [x] #7 All previously-existing first-seen, valueChanged, and 60 s heartbeat publish paths still set mqttPendingBitSlot/ByteSlot before returning true
- [x] #8 STATUS_HEARTBEAT_INTERVAL_SEC remains 60 and the intervalElapsed predicate is unchanged
- [x] #9 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash are staged with the firmware change
- [x] #10 python build.py --firmware exits 0
- [x] #11 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [x] #12 No code comment in OTGW-Core.ino still references MQTT_GATED_PUBLISH_SPACING_MS, mqttLastGatedPublishMs, or the rate-gate as an active mechanism (references in /// blocks may explain the removal under ADR-076)
- [x] #13 ADR-076 is cited in at least one code comment near the deleted region as the authority for the removal
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read OTGW-Core.ino lines 252-310 (region 1) and lines 1543-1627 (region 2) to confirm exact textual surface of the deletions.\n2. Remove static uint32_t mqttLastGatedPublishMs and static constexpr uint32_t MQTT_GATED_PUBLISH_SPACING_MS at lines 289-290.\n3. Remove the TASK-402 comment block (lines 278-288) that explains the rate-gate.\n4. Remove the rate-gate if-block in shouldPublishTrackedStatusBit() (lines 1572-1577) and the trailing mqttLastGatedPublishMs assignment (line 1579).\n5. Remove the rate-gate if-block in shouldPublishTrackedStatusByte() (lines 1616-1621) and the trailing mqttLastGatedPublishMs assignment (line 1623).\n6. Remove mqttLastGatedPublishMs = 0 from resetMqttTrackedState() (line 437).\n7. Update the surrounding code comments to reference ADR-076 and remove stale TASK-402/612 explanations.\n8. Run python build.py --firmware; verify exit 0.\n9. Run python evaluate.py --quick; verify no new failures.\n10. Bump prerelease via bin/bump-prerelease.sh; stage version.h + data/version.hash.\n11. Commit. Defer push until TASK-643 also lands so both arrive under one beta tag.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed in one pass.
- Removed lines 289-290 (mqttLastGatedPublishMs + MQTT_GATED_PUBLISH_SPACING_MS) and the surrounding TASK-402 comment block.
- Removed the rate-gate if-block plus the mqttLastGatedPublishMs assignment from both shouldPublishTrackedStatusBit() and shouldPublishTrackedStatusByte().
- Removed the mqttLastGatedPublishMs = 0 reset from resetMqttTrackedState().
- Replaced the deleted comments with concise ADR-076 references.
- Bumped prerelease beta.7 → beta.8 via bin/bump-prerelease.sh.
- python build.py --firmware: exit 0, artifact OTGW-firmware-1.6.0-beta.8 produced.
- python evaluate.py --quick: 34/36 passed, 2 info, 0 failures, 0 warnings, Health 100%.
- ADR-076 status flipped from Proposed to Accepted in the same commit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the 250 ms global MQTT rate-gate (TASK-402 / TASK-612) from the status fan-out.

The global rate-gate token was meant to spread the 60 s heartbeat storm across slots, but it actually starved per-bit slots: every sibling slot in a single processOT() fan-out hit intervalElapsed=true at the same heartbeat boundary, the combined byte slot grabbed the token first, and the bit slots lost it for the next heartbeat too. Bits 2–5 of OT_Statusflags entered Home Assistant as `unknown` as a direct consequence.

Changes (src/OTGW-firmware/OTGW-Core.ino):
- Removed `static uint32_t mqttLastGatedPublishMs` and `static constexpr uint32_t MQTT_GATED_PUBLISH_SPACING_MS = 250` (was at lines 289-290).
- Removed the rate-gate if-block + `mqttLastGatedPublishMs = nowMs` assignment from `shouldPublishTrackedStatusBit()`.
- Removed the same pair from `shouldPublishTrackedStatusByte()`.
- Removed the `mqttLastGatedPublishMs = 0` reset from `resetMqttTrackedState()`.
- Replaced the TASK-402 / TASK-612 comments with concise ADR-076 references.

After this change the predicate for every gated slot reduces to ADR-052's per-slot contract:

  publish = firstSeen OR valueChanged OR (now - lastPublished ≥ 60 s)

Heap-tier back-pressure via canPublishMQTT() (ADR-030) remains the sole publish throttle.

Verification:
- python build.py --firmware: exit 0, artifact OTGW-firmware-1.6.0-beta.8.
- python evaluate.py --quick: 34 pass / 0 fail / 0 warn / 2 info / Health 100%.

Risks / follow-ups:
- TASK-643 (companion ADR-076 Decision item 6) still needs to land before silent failures under heap pressure are fully closed.
- Field validation under beta.8 will confirm bits 2-5 now publish on the 60 s heartbeat.
<!-- SECTION:FINAL_SUMMARY:END -->
