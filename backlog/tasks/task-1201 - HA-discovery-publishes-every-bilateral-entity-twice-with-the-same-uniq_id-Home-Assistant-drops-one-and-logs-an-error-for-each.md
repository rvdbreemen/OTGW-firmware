---
id: TASK-1201
title: >-
  HA discovery publishes every bilateral entity twice with the same uniq_id;
  Home Assistant drops one and logs an error for each
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-03 14:00'
updated_date: '2026-10-03 14:06'
labels:
  - mqtt
  - ha-discovery
  - bug
dependencies: []
priority: medium
ordinal: 323000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-03 on the laptop test rig (Home Assistant 2026.6.3 plus Mosquitto) with the OTGW32 bench on 2.0.0-alpha.404+220fca7.

SYMPTOM. After a discovery republish, the broker holds two retained config topics for each bilateral entity, for example homeassistant/binary_sensor/<node>/boiler_ch_enable/config and .../thermostat_ch_enable/config. Both carry the same uniq_id (<node>-otd_ch_enable) and the same state topic, and are identical apart from the device block. Home Assistant keeps whichever config arrives first and logs 'Platform mqtt does not generate unique IDs. ID ... already exists - ignoring' for the other: 109 such errors within 4 minutes of one republish, and 179 duplicated uniq_ids on the rig broker afterwards (counted with the rig script disc_dup_repro.py). No value is lost: both configs point to the same state topic.

CAUSE (from code, to confirm in the fix). MQTTHaDiscovery.cpp builds the config object_id with haDeviceShortName(ctx) (boiler_/thermostat_, TASK-648 Task 4, ':2612'). It builds the uniq_id as <nodeId>-<haSourcePrefix(ctx.device, ctx)><idLabel>[<sourceSuffix>] (':2405-2416'). Since ADR-140 the per-device suffix in uniq_id was replaced by a source/engine prefix, which is the same for the Boiler and the Thermostat device (otd_ on OT-Direct, pic_ on PIC). An entity that is streamed once per side therefore gets two config topics with one uniq_id. Under ADR-140 there is one HA device, so the second stream adds nothing.

COST. Twice the discovery traffic for these entities; error noise in every user's HA log at each HA start or republish; and a migration hazard. Clearing a retained config that HA bound the entity to removes the entity (and its customisations), and which of the two configs HA bound depends on arrival order.

Related: ADR-140 (single-device topology), ADR-077 (streaming discovery), ADR-100 (JIT discovery), TASK-648, TASK-871, TASK-1037 (discovery heal / orphan detection).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced old-vs-fix on the laptop rig from a clean broker: the node's retained discovery configs are cleared, a full republish drains (disc_pending_ids 0), and the duplicated-uniq_id count is N>0 on current dev and 0 on the fix
- [ ] #2 Each entity is announced on exactly one config topic in single-device mode; no uniq_id, entity name or state topic changes, so existing Home Assistant entities keep their entity_id and customisations
- [ ] #3 A migration path for the duplicate retained configs already on users' brokers is chosen with the maintainer and verified on the rig HA: after the upgrade and an HA restart, no OTGW entity was removed or renamed, and the HA log shows no 'does not generate unique IDs' error for the node
- [ ] #4 Build green for esp32, esp32-classic and esp32-combo (fresh artifacts); python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 OLD side, 2026-10-03: OTGW32 on 2.0.0-alpha.404+220fca7 (current dev firmware), laptop rig (Mosquitto plus Home Assistant 2026.6.3). Tool: disc_dup_repro.py (out of the repo). Sequence:
1. Wait until disc_pending_ids is 0.
2. Clear every retained homeassistant/+/otgw-1020BA21B4F8/+/config topic, and check that 0 are left.
3. POST /api/v2/discovery/republish, which answered 200 marked_pending count=130.
4. Wait for the drip to finish (disc_pending_ids 0 after 269 s).
5. Dump the retained configs.
Result: 632 config topics, 391 distinct uniq_ids, 239 uniq_ids on more than one config topic. All 239 pairs are identical apart from the device block; each is a boiler_<label> / thermostat_<label> pair, for example otd_ch2_enable, otd_vh_bypass_mode and otd_vh_fault. Home Assistant logged 320 'does not generate unique IDs' errors over the run. Evidence: %LOCALAPPDATA%/OTGW-capture/task1201/old-alpha404/ (configs_after.json, dups.json, ha_log_unique_id_errors.txt). The fix runs in lane wt-1201.
<!-- SECTION:NOTES:END -->
