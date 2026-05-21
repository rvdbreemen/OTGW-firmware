---
id: TASK-644
title: >-
  Apply ADR-076 confirmation-relocation pattern to mqttPendingSlot
  (normal-msgId)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 07:52'
updated_date: '2026-05-21 07:55'
labels:
  - mqtt
  - adr-076-pattern
  - bugfix
  - beta
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bug fix within the existing pattern established by ADR-076: the normal-msgId pending slot (mqttPendingSlot) has the same stale-pending defect as the bit/byte pending slots that ADR-076 fixed. A heap-throttled sendMQTTData() inside an OTPublishGate block leaves mqttPendingSlot.pending=true even though no publish reached the broker; an unrelated successful sendMQTTData() between then and the next shouldPublishMQTTForID/PSField call commits the stale pending into mqttlastsent[], suppressing the next valueChanged/intervalElapsed publish for that msgId. ADR-076 scoped the fix to bit/byte explicitly; this task extends the same pattern to mqttPendingSlot. No new ADR required since the pattern is already accepted and this is a strict-pattern bug fix per CLAUDE.md ("Do NOT create ADRs for: bug fixes within existing patterns").
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Global uint32_t mqttSendSuccessCount declared in OTGW-firmware.h and defined in MQTTstuff.ino; incremented at the end of each sendMQTTData() success path (both overloads that actually publish)
- [x] #2 confirmMQTTPublishSlot() call is removed from both sendMQTTData() success paths in MQTTstuff.ino — sendMQTTData no longer commits any pending slot record
- [x] #3 Normal-msgId publish site (OTGW-Core.ino around line 4185) wraps the OTPublishGate block with: capture pre-block mqttSendSuccessCount → run gate+decodeAndPublishOTValue → if mqttPendingSlot.pending, commit via confirmMQTTPublishSlot() when count advanced, else clear .pending=false
- [x] #4 PS=1 publish site (OTGW-Core.ino around line 3714) wraps its OTPublishGate block with the same commit-or-clear logic
- [x] #5 No remaining call to confirmMQTTPublishSlot() inside sendMQTTData() — grep confirms only the two new wrapper sites call it
- [x] #6 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash staged with the firmware change
- [x] #7 python build.py --firmware exits 0
- [x] #8 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [x] #9 Code comments cite ADR-076 as the authority for the pattern even though this is the third application of it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add extern uint32_t mqttSendSuccessCount; to OTGW-firmware.h after the sendMQTTData declarations.
2. Define uint32_t mqttSendSuccessCount = 0; at file scope in MQTTstuff.ino.
3. In both sendMQTTData() success paths (the void to bool refactor done in TASK-643): increment mqttSendSuccessCount before return true, and remove the confirmMQTTPublishSlot() call.
4. Inline at OTGW-Core.ino line 4185 region: wrap OTPublishGate block with success-count capture and post-block commit-or-clear of mqttPendingSlot.
5. Same at line 3714 region for PS=1 path.
6. Verify grep that confirmMQTTPublishSlot is only called from the two wrapper sites and from resetMqttTrackedState if it does any reset.
7. Bump prerelease beta.9 to beta.10.
8. python build.py --firmware; exit 0.
9. python evaluate.py --quick; no new failures.
10. Commit.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed.

- Added uint32_t mqttSendSuccessCount = 0 at file scope in MQTTstuff.ino, just above sendMQTTData. extern declared in OTGW-firmware.h with ADR-076-pattern comment.
- Both sendMQTTData success paths now `++mqttSendSuccessCount` instead of confirmMQTTPublishSlot().
- OTGW-Core.ino line 3714 area (PS=1 path) and line 4185 area (normal OT decode path) now capture preSuccessCount before the OTPublishGate scope, run the publish path, and after the gate scope commit-or-clear mqttPendingSlot based on whether the counter advanced.
- Grep confirms confirmMQTTPublishSlot is now called only from those two OTPublishGate wrapper sites plus its definition.
- Prerelease beta.9 → beta.10.
- python build.py --firmware: exit 0, OTGW-firmware-1.6.0-beta.10 artifact.
- python evaluate.py --quick: 34 pass / 0 fail / 0 warn / 2 info / Health 100%.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed the third instance of the ADR-076 stale-pending defect — this time on mqttPendingSlot (normal-msgId throttle).

Problem: shouldPublishMQTTForID() and shouldPublishMQTTForPSField() install mqttPendingSlot.pending=true inside an OTPublishGate block. Multiple sendMQTTData() calls may follow within that block (base topic + source-separated topic + flag8 expansions). Previously, the first successful sendMQTTData inside the block committed the pending via confirmMQTTPublishSlot(). If all sends in the block failed (heap throttle), pending stayed dirty, and a later unrelated successful sendMQTTData() (e.g. heartbeat, discovery drip) silently committed the stale pending — advancing mqttlastsent[idx] without HA receiving the value.

Fix: identical structural pattern to ADR-076 Decision item 6 (bit/byte fix), adapted for the multi-publish normal-msgId path.

Changes:
- src/OTGW-firmware/OTGW-firmware.h: extern uint32_t mqttSendSuccessCount, with comment explaining the OTPublishGate caller contract.
- src/OTGW-firmware/MQTTstuff.ino: defined the counter; both sendMQTTData() success paths increment it instead of calling confirmMQTTPublishSlot().
- src/OTGW-firmware/OTGW-Core.ino lines ~3714 and ~4185: each OTPublishGate block now captures preSuccessCount, runs the publish, and either commits (if mqttSendSuccessCount advanced) or clears mqttPendingSlot.pending=false.

No new ADR. Per CLAUDE.md: "Do NOT create ADRs for: bug fixes within existing patterns" — this is the same pattern ADR-076 already authorized, extended to a third slot type.

Verification:
- python build.py --firmware: exit 0, artifact OTGW-firmware-1.6.0-beta.10.
- python evaluate.py --quick: 34 pass / 0 fail / 0 warn / 2 info / Health 100%.

All three slot-confirmation paths (bit, byte, normal) now follow the same contract: pending is scoped to the publish that installed it; an unrelated sendMQTTData() can never silently commit a stale pending record.
<!-- SECTION:FINAL_SUMMARY:END -->
