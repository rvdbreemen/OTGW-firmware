---
id: TASK-1460
title: >-
  sat_ovp_enabled missing from kRetiredSatEntities: stale HA entity stays on
  brokers
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T07:19:00Z'
updated_date: '2026-10-29T13:02:00Z'
labels:
  - sat
  - mqtt
  - ha-discovery
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 620000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the sat/* MQTT.md audit (docs session, 2026-10-29, dev 549e93312); code read only, no device. OVP was removed in ad8f7fb9d (TASK-891.6) but sat_ovp_enabled is not in kRetiredSatEntities (MQTTHaDiscovery.cpp:3744-3748), so brokers keep its retained discovery config. Check the other removed OVP entities too.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The OVP entities are listed in kRetiredSatEntities and test/host/test_ha_discovery_json.py covers their empty retained publish
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
ad8f7fb9d (TASK-891.6) removed one HA entity, the switch -sat-ovp-enabled (object id sat_ovp_enabled, state/command /sat/ovp_enabled), plus two retained state topics published until then: sat/ovp_enabled and sat/ovp_value (publishIfChangedB/F with retain, SATcontrol.ino:2874/2889 at ad8f7fb9d^). History search for ovp/opv in the discovery files found no other entity. Add two kRetiredSatEntities rows: the switch with its state topic, and a state-only row for sat/ovp_value (component and objectId nullptr; clearRetiredSatEntities skips the config when they are nullptr). Extend test/host/test_ha_discovery_json.py: the derived RETIRED_SAT_TOPIC_TAILS reads state-only rows, and a TASK-1460 check next to 1332/1341/1282.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29 evidence. Red on 3ea094e9d: python test/host/test_ha_discovery_json.py -> exit 1, RESULT: FAIL, only the new TASK-1460 check NO (no empty retained publish to switch/<node>/sat_ovp_enabled/config, sat/ovp_enabled, sat/ovp_value). Green on the fix (rebased on 8176c49a7): exit 0, RESULT: PASS, 13 checks yes, 0 NO. tests/test_sat_pwm_override.py pinned the retired-row count at 3; changed to check the force_pwm row itself (73 OK). All tests/test_sat_*.py OK; evaluate --quick 86 checks, 0 fail, 0 warn.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT HA discovery cleanup now also empties the retired OVP switch: kRetiredSatEntities gains { switch, sat_ovp_enabled, /sat/ovp_enabled } and a value-only row { nullptr, nullptr, /sat/ovp_value } (the clear skips the config topic when component/objectId are nullptr).

Shipped: 00540131d, 2.0.0-alpha.531 (pushed to origin/dev; covered by the posted zip OTGW-firmware-esp32-combo-2.0.0-alpha.532+689cb05-flash.zip, Discord msg 1565349517736546385).

Evidence AC#1 (this session, after the replay onto 103255a2f): test/host/test_ha_discovery_json.py, current harness against the 103255a2f MQTTHaDiscovery.cpp: 'TASK-1460 FIX ... sat_ovp_enabled/config, .../sat/ovp_enabled and .../sat/ovp_value: NO', RESULT: FAIL; against the fix: 'yes', RESULT: PASS. All 31 tests/test_sat_*.py + the harness exit 0; evaluate.py --quick 0 FAIL, 100%. Build esp32-combo (4 jobs) from 689cb05b1: fresh .ino.bin/.littlefs.bin/zip 13:57-13:59, .ino.bin contains alpha.532 and 689cb05 and the sat_ovp_enabled id (build.bat output was not capturable, so the SUCCESS line is not quoted). Replayed diff identical to the original 64308043d apart from banners.

Not verified: on a live broker/HA that the stale switch entity disappears (needs a unit that ran an OVP-era build).
<!-- SECTION:FINAL_SUMMARY:END -->
