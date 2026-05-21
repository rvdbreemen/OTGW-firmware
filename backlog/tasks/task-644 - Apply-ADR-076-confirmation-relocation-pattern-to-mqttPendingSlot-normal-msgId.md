---
id: TASK-644
title: >-
  Apply ADR-076 confirmation-relocation pattern to mqttPendingSlot
  (normal-msgId)
status: To Do
assignee: []
created_date: '2026-05-21 07:52'
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
