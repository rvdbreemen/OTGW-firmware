---
id: TASK-1035
title: >-
  Fix: gateway_mode + otgw_connected HA discovery never enqueued at boot (faux
  id 244)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-09 19:23'
updated_date: '2026-07-09 19:32'
labels: []
dependencies: []
ordinal: 155000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 1.7.1 gateway_mode/otgw_connected binary-sensors (pseudo-ID 244) are added to the HA discovery table but publishNonOTDiscoveryConfigs() never calls setMQTTConfigPending(244), so the discovery configs are never published on boot/reconnect and the entities never appear in Home Assistant (only via manual /republish). Found by adversarial review of v1.7.0..HEAD; matches the faux-sensor-id-boot-publish trap (TASK-942).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A named pseudo-ID constant for 244 exists (no bare literal)
- [x] #2 publishNonOTDiscoveryConfigs() enqueues the 244 constant so gateway_mode + otgw_connected discovery configs publish on boot and reconnect
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added named pseudo-ID constant OTGWconnstatusid=244 (OTGW-firmware.h) and enqueued it in publishNonOTDiscoveryConfigs() (MQTTstuff.ino) so the gateway_mode + otgw_connected binary-sensor discovery configs publish on boot and reconnect. Build via build.bat: SUCCESS (72% flash). evaluate.py --quick: 97.3%, no new failures (the 1 failure is a pre-existing ghost ADR ref).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixes the v1.7.1 defect where the gateway_mode and otgw_connected HA binary-sensors (faux dataid 244) were added to the discovery table but never queued for publication on any automatic path, so they never appeared in Home Assistant (only via a manual /republish).

Root cause: publishNonOTDiscoveryConfigs() enqueues pending discovery bits for the other pseudo-IDs (247 heapstats, 248 fwinfo, etc.) but not 244; the JIT bus path only enqueues real OT-bus IDs; and the ADR-062 verify self-heal keys off published-topic count so a never-published entry is never flagged missing.

Fix: add byte OTGWconnstatusid = 244 (OTGW-firmware.h) and setMQTTConfigPending(OTGWconnstatusid) in publishNonOTDiscoveryConfigs() (MQTTstuff.ino). The discovery table literal 244 is unchanged (static-aggregate initializer, consistent with sibling pseudo-ID rows).

Found by adversarial multi-agent review of v1.7.0..HEAD; matches the documented faux-sensor-id-boot-publish trap (TASK-942). Build SUCCESS, evaluator green.
<!-- SECTION:FINAL_SUMMARY:END -->
