---
id: TASK-562
title: 'feat-2.0.0: port TASK-561 — ADR-066 source-topic gate fix (Write-Ack flapping)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 13:17'
updated_date: '2026-05-07 13:24'
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
- [ ] #1 Predicate at MQTTstuff.ino:1565 is replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
- [ ] #2 Comment block immediately above the gate is updated so a future reader can see why OTdata.type (not rsptype) is the right field
- [ ] #3 Build (firmware + filesystem) for the 2.0.0 target exits 0
- [ ] #4 evaluate.py --quick shows no new failures
- [ ] #5 Prerelease bump committed alongside the firmware change per the 2.0.0 worktree's bump policy
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
