---
id: TASK-1991
title: >-
  MQTT: a retained <label>/override topic left from before a reboot is never
  cleared
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T22:35:00Z'
updated_date: '2026-11-16T23:29:00Z'
labels:
  - mqtt
  - bug
dependencies: []
priority: low
ordinal: 1014000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Edge left by TASK-1985 (coordinator 2026-11-15: separate, low). The ADR-118 override store is RAM only; after a reboot it is empty, so a <label>/override topic that was retained before the reboot is never cleared by publishActiveOverrides() (it only clears entries it holds). HA keeps showing that old value. Possible approach: at MQTT connect (connect burst), publish an empty retained message once for each label that can carry an override (the ids recordOTOverride() is called for), before the first periodic publish; an override still active re-publishes its value within 60 s. Check the burst windowing rules (ADR-088) for the connect path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a reboot with a retained <label>/override on the broker and no active override, the topic is gone within the first minute after connect (host test on the connect path, red on the current code)
- [x] #2 An override active after the reboot is published again within the 60 s timer
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-17 (95, coordinator: code + host test now, build with TASK-2007 later; lane wt-95-2007). Fix: OTGW-Core.ino startStaleOverrideClear() / staleOverrideClearStep() (between '--- staleOverrideClear begin/end'): after every MQTT connect one sweep over OTmap sends an empty retained <label>/override (removes a retained topic, a no-op where none exists) for each ot_f88 id, the only ids recordOTOverride() is called for (print_f88()), skipping ids that have an entry in the override store (an active override keeps its topic; publishActiveOverrides() refreshes it within 60 s) and ids without a label; one topic per step, a failed send retries the same id. MQTTstuff.ino: connStageStart() starts the sweep at every connect; staleOverrideClearTick() on MQTT_STATE_IS_CONNECTED runs one step after the four connect portions are done (sConnStage == 0), at least 100 ms apart (OVR_CLEAR_GAP_MS) and only with the outbox at most CONN_STAGE_MAX_OUTBOX (2), so the ~38 f88 labels go out over about 4 s, not as a burst (the TASK-2004 lesson). Host test tests/test_mqtt_override_reboot_clear.py on the real functions with a stub OTmap (TSet/Toutside f88, SConfig flag8) and a stub broker that keeps retained topics: FIX 5/5; OLD (origin OTGW-Core.ino/.h) 2 FAIL: 'TSet/override' still retained 60 s after connect. tests/test_mqtt_connect_staging.py got stubs for the two new functions. All 24 tests that read MQTTstuff.ino PASS; 11 tests on the override/connect functions PASS; evaluate.py --quick 88/0. Not verified: on hardware (build with 2007; bench: put a retained TSet/override on the broker, reboot, check it is gone within a minute and an active OT= override comes back).
<!-- SECTION:NOTES:END -->
