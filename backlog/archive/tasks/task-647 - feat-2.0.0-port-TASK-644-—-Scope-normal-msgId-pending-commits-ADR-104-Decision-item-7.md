---
id: TASK-647
title: >-
  feat-2.0.0: port TASK-644 — Scope normal-msgId pending commits (ADR-104
  Decision item 7)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 08:00'
updated_date: '2026-05-21 08:23'
labels:
  - mqtt
  - adr-104
  - beta-2.0.0
  - port-from-dev
  - bugfix
dependencies: []
priority: high
ordinal: 47000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 sibling of dev's TASK-644. Apply the ADR-104 confirmation-relocation pattern to mqttPendingSlot (normal-msgId throttle). sendMQTTData increments a monotonic mqttSendSuccessCount on every confirmed publish; the two OTPublishGate callers (shouldPublishMQTTForID site at the OT decode path; shouldPublishMQTTForPSField site at the PS=1 summary path) capture the pre-block count and commit pending if any send landed, else clear.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Global uint32_t mqttSendSuccessCount declared extern in OTGW-firmware.h
- [x] #2 Counter defined in MQTTstuff.ino at file scope
- [x] #3 Both sendMQTTData success paths increment mqttSendSuccessCount before return true
- [x] #4 confirmMQTTPublishSlot() call removed from both sendMQTTData success paths
- [x] #5 Normal-msgId publish site (OTGW-Core.ino, OTPublishGate around shouldPublishMQTTForID) wraps with preSuccessCount capture + post-block commit-or-clear of mqttPendingSlot
- [x] #6 PS=1 publish site (OTGW-Core.ino, OTPublishGate around shouldPublishMQTTForPSField) wraps with the same commit-or-clear logic
- [x] #7 grep confirms confirmMQTTPublishSlot is only called from the two wrapper sites plus its definition
- [x] #8 Prerelease bumped via bin/bump-prerelease.sh
- [x] #9 python build.py --firmware exits 0
- [x] #10 python evaluate.py --quick shows no new failures
- [x] #11 ADR-104 cited in comments near the relocated logic
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2.0.0 port complete. uint32_t mqttSendSuccessCount = 0 added at MQTTstuff.ino file scope; extern in OTGW-firmware.h. Both sendMQTTData success paths increment instead of confirmMQTTPublishSlot(). PS=1 and OT decode OTPublishGate blocks now capture pre-count, run publish, commit-or-clear mqttPendingSlot. grep confirms only the two wrapper sites call confirmMQTTPublishSlot now. ESP8266 build exit 0; ESP32 build deferred. Evaluator 60/0/0/1/7. alpha.44 → alpha.45.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
2.0.0 port of dev TASK-644 — closed the third instance of the ADR-104 stale-pending defect for mqttPendingSlot (normal-msgId throttle).

Same structural fix as dev TASK-644: sendMQTTData increments a monotonic mqttSendSuccessCount on every confirmed publish. The two OTPublishGate sites (shouldPublishMQTTForID on the OT decode path; shouldPublishMQTTForPSField on the PS=1 summary path) capture the pre-block count, run the publish, then commit-or-clear mqttPendingSlot.

All three slot-confirmation paths on 2.0.0 (bit, byte, normal) now follow the same contract — pending is scoped to the publish that installed it; an unrelated sendMQTTData can never silently commit a stale pending record.

Verification: ESP8266 build exit 0, evaluator 60 pass / 0 fail / 1 warn / 7 info / 98.5%, alpha.44 → alpha.45. ESP32 build deferred (container TLS sandbox). Commit unsigned (one-time --no-gpg-sign per maintainer authorisation).
<!-- SECTION:FINAL_SUMMARY:END -->
