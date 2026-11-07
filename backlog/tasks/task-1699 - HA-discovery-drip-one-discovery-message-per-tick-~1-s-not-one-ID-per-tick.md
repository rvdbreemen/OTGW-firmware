---
id: TASK-1699
title: 'HA discovery drip: one discovery message per tick (~1 s), not one ID per tick'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T16:56:00Z'
updated_date: '2026-11-07T21:55:00Z'
labels:
  - mqtt
  - ha
  - heap
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 813000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: drip out the HA discovery on boot one MQTT discovery message at a time, paced ~1 s apart, as intended on 1.x. Today (2.x and 1.x alike) loopMQTTDiscovery publishes one pending ID per tick every DISCOVERY_INTERVAL_NORMAL = 2 s (10 s under heap pressure; MQTTstuff.ino:2364-2365, 'one per tick'), but one ID can carry many entity configs: OTid 0 streams the status-bit binary sensors, two climate entities and all SAT switch/select/number entities in one tick (MQTTstuff.ino:2958-2975). beta.123 diag on .61: MQTT outbox high-water 48-49 messages after every boot (TASK-1679), estimated 4-10 KB. Change the drip to a per-entity cursor: one config per tick at ~1 s, ADR-088 burst windows and cooldown unchanged, ADR-077 streaming kept. Related: TASK-1697 (MQTT pcb cap/outbox bound).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Boot on .61: hd_mqtt_queue_hwm during the discovery drip <= a small bound (e.g. 3) instead of 48-49, and every entity still appears in HA (count before/after equal)
- [ ] #2 Host test: a pending OTid 0 publishes one entity config per tick (red on old code: many per tick)
- [ ] #3 ADR-088 gates in evaluate.py green; full discovery duration measured and documented
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Per-entity drip cursor (KISS, semantics-preserving).
1. MQTTstuff.ino: file-static DiscoveryEmitGate {limit, skip, seen, emitted, more, ran} + discoveryTakeSlot(): full mode (limit=false) always true (doAutoConfigure() and the harness parts A-E unchanged); drip mode returns true for exactly one slot index == skip, sets more when a later slot exists.
2. doAutoConfigureMsgid(OTid,isFirst) keeps its signature (harness anchor); every emitted unit is wrapped: if (discoveryTakeSlot() && streamX()) result = true. Slot is taken BEFORE applicability checks (dhw switch, skipReplaced) so slot indices stay stable between ticks even when MsgID 3 arrives mid-drip. Source variants: expandAndStreamSensorSources gets a variant index (one config per slot). clearRetiredSatEntities becomes per-index (retiredSatEntityCount + clearRetiredSatEntityAt) so its ~25 empty retained deletes also go one per slot. PIC 244: button + 8 selects one slot each, allOk accumulated across ticks.
3. New doAutoConfigureMsgidStep(OTid,isFirst,cursor): runs one slot; returns More/Done/Retry (Retry = an early guard returned before any slot: lock, broker, heap; cursor unchanged). Accumulates anyOk (id 244: allOk) across ticks; at Done success=accumulator; failure restarts the id at slot 0 next tick (same as old whole-id retry). dripDeviceInfoPending cleared after the first tick whose slot published OK.
4. loopMQTTDiscovery calls the step; pending bit cleared only at Done+success. DISCOVERY_INTERVAL_NORMAL 2 -> 1 s (Robert: ~1 s apart); slow 10 s, burst defer and cooldown (ADR-088) unchanged.
Verification: harness test/host/test_ha_discovery_json gets part F (built when doAutoConfigureMsgidStep exists): drive the step for every id under the 4 perms; assert <=1 config publish per step and that the union of step-mode topics/payloads equals the full-mode set (every entity still appears). New tests/test_discovery_drip_per_entity.py: red on old (part F absent: per-tick pubs of id 0 = full-call count, many), green on fix. evaluate --quick (ADR-088 gates). Bench .61: boot, hd_mqtt_queue_hwm during drip vs 48-49 before, HA entity count before/after on the bench broker, drip duration measured from telnet [drip] log. No ADR: no topic/payload contract change; noted against ADR-077/088/140.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 implementation (main tree, uncommitted). Advisor review before coding: cursor keyed to the OT id and reset on id change/markAll/MQTT drop; one PUBLISH per tick (a slot that publishes nothing passes to the next slot in the same tick); gate on the caller's stack, not a static (ADR-090); doAutoConfigureMsgid(OTid,isFirst) kept as a wrapper for the harness anchor.

Opus code review findings, all fixed: (1) id 255 now all-or-nothing like 244; (2) a slot that fails while discovery cannot publish (broker, publish gate, heap < HA_DISCOVERY_STREAM_HEAP_MIN 4000) ends the tick and is retried (Retry), and the drip guard waits above the composers' heap floor, so a heap dip mid-id no longer marks the rest done; (3) zones, both source variants and PV telemetry take their slots unconditionally, so slot numbers do not shift when a zone times out or bSeparateSources/bPvBoostEnabled change mid-drip; (4) empty retained clears do not count as the first config (device block, ADR-140); (5) dripDueWithinMs() returns false when no discovery is pending, so the 1 s drip timer does not defer PR= queries; (6) stale 2 s comments updated. Not changed: an entity whose publish fails while publishing is possible (outbox full) is passed, as before.

Evidence so far. Host (real composers via test/host/test_ha_discovery_json part F): tests/test_discovery_drip_per_entity.py 7 OK: <=1 config per step for every id under 4 permutations; drip sends exactly the full pass's publishes (device block normalised: the full pass repeated the full block on every binary sensor of id 0 because streamBinarySensorDiscovery leaves ctx.isFirstEntity set; the drip sends it once, on step 0 of id 0, as ADR-140 states); verdicts equal; broker drop mid-id -> Retry x3 then complete; zone timeout / bSeparateSources off mid-drip -> no skip. Red on old (OTGW_DISC_REV=HEAD): one drip tick of id 0 = 39-40 configs. Mutants: zone/sources slot-after-check -> shift test FAILS; no retry on gate close -> drop test FAILS. Harness RESULT PASS, test_sat_health_discovery 6 OK, test_ble_discovery_1462 5 OK, evaluate --quick 85/0. Bench baseline .61 beta.128 after reboot: hd_mqtt_queue_hwm 49 (48 on a first reboot), 193 configs + 43 empty clears in 32 s, peak 39 configs in one second (mosquitto_sub on homeassistant/# from before the reboot, filtered on otgw-1020BA21B4F8; log scratchpad b103/drip-old128.log).

Regression sweep 2026-11-07 (full run_tests.py was killed twice by low memory; suites rerun one by one). TASK-1699 broke two harness slices, both fixed: test_sat_pwm_override ForcePwmRemoved source assertion (now accepts clearRetiredSatEntityAt) -> 82 OK; test/host/test_dhw_water_meter (markAllMQTTConfigPending now resets sDripCursor; harness exports DiscoveryDripCursor + the static when present) -> RESULT PASS. Also green: test_sat_mqtt_contract_1485 15 OK, test_sat_health_discovery 6 OK, test_ble_discovery_1462 5 OK, harness test_ha_discovery_json PASS, drip test 7 OK, evaluate 85/0. Same commit fixes test/host/test_cmd_queue_drop (did not compile since TASK-1695: two stubs) -> PASS. Red before TASK-1684/1695 (verified at 834902ac8~1), not touched: test_boiler_unsupported_origin P1 (TASK-1713), test_local_frame_consumers AC5/AC6 (TASK-1714); env-only: test_board_detect_persist_gate (Defender WinError 225), test_web_file_scheduling (no host g++), test_diag_decode (fixture recording-v1.bin missing); test_sat_no_return_startup otCurrentMode fixed in 54's TASK-1707 (local).
<!-- SECTION:NOTES:END -->
