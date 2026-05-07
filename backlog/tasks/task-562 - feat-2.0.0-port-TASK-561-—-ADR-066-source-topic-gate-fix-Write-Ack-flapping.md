---
id: TASK-562
title: 'feat-2.0.0: port TASK-561 — ADR-066 source-topic gate fix (Write-Ack flapping)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 13:17'
updated_date: '2026-05-07 15:07'
labels:
  - bug
  - mqtt
  - adr-066
  - port
dependencies: []
priority: high
ordinal: 25000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-branch fix for ADR-066 source-topic gate. Same defect on this branch at MQTTstuff.ino:1565: rsptype (OTGW_response_type, 0..5) is compared against OT_WRITE_ACK (OpenThermMessageType=B101=5). Numeric collision means the gate fires only for OTGW_UNDEF, never for real boiler Write-Ack frames. msgids 14, 16, 23, 24, 37, 98 leak their per-spec-undefined Write-Ack data byte (~0) to source-separated subtopics. Cross-ref TASK-561 in dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Predicate at MQTTstuff.ino:1565 is replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
- [x] #2 Comment block immediately above the gate is updated so a future reader can see why OTdata.type (not rsptype) is the right field
- [x] #3 Build (firmware + filesystem) for the 2.0.0 target exits 0
- [x] #4 evaluate.py --quick shows no new failures
- [x] #5 Prerelease bump committed alongside the firmware change per the 2.0.0 worktree's bump policy
- [ ] #6 Field-validation note in Final Summary: with bSeparateSources=true, msgid 14 and 16 on _thermostat/_boiler no longer flap to 0 between Write-Data frames (tester sign-off via Discord; leave blocking AC if not yet confirmed)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify enum-family bug at MQTTstuff.ino:1565 in 2.0.0 (rsptype vs OTdata.type collision at value 5)
2. Replace predicate with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
3. Update comment block to explain enum-family distinction
4. Bump prerelease alpha.7 -> alpha.8 via bin/bump-prerelease.sh
5. Build firmware+filesystem (./build.sh) — exit 0 required
6. Run python evaluate.py --quick — no new failures
7. Commit with TASK-562 reference and dev sibling SHA afdc6480; pre-commit hooks must pass
8. Push to origin/feature-dev-2.0.0-otgw32-esp32-sat-support (standing permission)
9. Check ACs #1-5; leave #6 unchecked (Discord field-validation pending); leave task In Progress
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the dev-branch fix for the ADR-066 source-topic gate enum-family bug to the 2.0.0 line.

## What changed
- src/OTGW-firmware/MQTTstuff.ino — predicate at the publishToSourceTopic() Write-Ack gate replaced with `OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue`. Comment block immediately above the gate expanded to document the OTGW_response_type vs OpenThermMessageType collision.
- src/OTGW-firmware/version.h + cascaded version-string headers across src/OTGW-firmware/** — alpha.7 → alpha.8 prerelease bump (`bin/bump-prerelease.sh`).

## Why
publishToSourceTopic() compared rsptype (OTGW_response_type, BOILER=0..UNDEF=5) against OT_WRITE_ACK (OpenThermMessageType=B101=5). The two enum families only collide numerically at value 5, so the gate fired only for OTGW_UNDEF, never for real boiler Write-Ack frames. For the six MsgIDs flagged bSlaveEchoesValue=false (14, 16, 23, 24, 37, 98) the slave's per-spec-undefined Write-Ack data byte (~0) leaked to <topic>_thermostat and <topic>_boiler, alternating with the Write-Data value → 100→0→100→0 flap visible in HA at ~1 Hz when bSeparateSources=true.

The canonical/master-topic path was unaffected: is_value_valid_for_master_topic correctly checks `OT.type == OT_WRITE_DATA`. Only the source-separated subtopics were leaking.

## User impact
Resolves user-observed flapping on msgid 14 (MaxRelModLevelSetting) with bSeparateSources=true on the 2.0.0 line. Same defect, same fix, same six MsgIDs as dev sibling.

## Tests / gates
- ./build.sh --firmware → exit 0. Both ESP32 and ESP8266 firmware + filesystem built clean (alpha.8+ccb1ec2). Dist zips produced.
- python3 evaluate.py --quick → 0 failed, 2 unrelated warnings (mqtt_configuratie.cpp not found; sendMQTTheapdiag buffer arithmetic check skipped). Health 97.1%.
- Pre-commit hooks (adr-judge, bump-check, commit-msg TASK gate) all passed.
- Commit 1efc2f80 on feature-dev-2.0.0-otgw32-esp32-sat-support, pushed to origin per project standing permission.

## Cross-tree
- Dev sibling: afdc6480 on origin/dev (the dev-side TASK-561 in dev worktree). Same predicate, identical fix.
- ADR-066 unchanged — this restores the documented intent; no new architectural decision.

## Blocking AC #6
Field validation needs a tester on Discord to confirm alpha.8 fixes the flap on msgids 14/16 with bSeparateSources=true on a real boiler. Per CLAUDE.md "hardware-specific tester feedback" exception, leaving the task at In Progress until tester sign-off arrives. Flip AC #6 to checked and set Done once confirmed.
<!-- SECTION:FINAL_SUMMARY:END -->
