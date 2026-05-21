---
id: TASK-644
title: >-
  Apply ADR-076 confirmation-relocation pattern to mqttPendingSlot
  (normal-msgId)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 07:52'
updated_date: '2026-05-21 07:53'
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
- [ ] #1 Global uint32_t mqttSendSuccessCount declared in OTGW-firmware.h and defined in MQTTstuff.ino; incremented at the end of each sendMQTTData() success path (both overloads that actually publish)
- [ ] #2 confirmMQTTPublishSlot() call is removed from both sendMQTTData() success paths in MQTTstuff.ino — sendMQTTData no longer commits any pending slot record
- [ ] #3 Normal-msgId publish site (OTGW-Core.ino around line 4185) wraps the OTPublishGate block with: capture pre-block mqttSendSuccessCount → run gate+decodeAndPublishOTValue → if mqttPendingSlot.pending, commit via confirmMQTTPublishSlot() when count advanced, else clear .pending=false
- [ ] #4 PS=1 publish site (OTGW-Core.ino around line 3714) wraps its OTPublishGate block with the same commit-or-clear logic
- [ ] #5 No remaining call to confirmMQTTPublishSlot() inside sendMQTTData() — grep confirms only the two new wrapper sites call it
- [ ] #6 Prerelease tag bumped via bin/bump-prerelease.sh and the bumped version.h + data/version.hash staged with the firmware change
- [ ] #7 python build.py --firmware exits 0
- [ ] #8 python evaluate.py --quick shows no new failures vs the pre-change baseline
- [ ] #9 Code comments cite ADR-076 as the authority for the pattern even though this is the third application of it
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add  to OTGW-firmware.h after the sendMQTTData declarations.\n2. Define  at file scope in MQTTstuff.ino.\n3. In both sendMQTTData() success paths (the void→bool refactor done in TASK-643): increment mqttSendSuccessCount before , and remove the confirmMQTTPublishSlot() call.\n4. Inline at OTGW-Core.ino line 4185 region: wrap OTPublishGate block with success-count capture and post-block commit-or-clear of mqttPendingSlot.\n5. Same at line 3714 region for PS=1 path.\n6. Verify grep that confirmMQTTPublishSlot is only called from the two wrapper sites and from resetMqttTrackedState (if it does any reset).\n7. Bump prerelease beta.9 → beta.10.\n8. python build.py --firmware; exit 0.\n9. python evaluate.py --quick; no new failures.\n10. Commit.
<!-- SECTION:PLAN:END -->
