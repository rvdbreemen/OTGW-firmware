---
id: TASK-561
title: 'fix: ADR-066 source-topic gate uses wrong enum family — Write-Ack flapping'
status: To Do
assignee: []
created_date: '2026-05-07 13:16'
labels:
  - bug
  - mqtt
  - adr-066
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
publishToSourceTopic() compares rsptype (OTGW_response_type, 0..5) against OT_WRITE_ACK (OpenThermMessageType=B101=5). The numeric collision means the gate fires only for OTGW_UNDEF, never for real boiler Write-Ack frames. Result: msgids 14, 16, 23, 24, 37, 98 publish their per-spec-undefined Write-Ack data byte (~0) to <topic>_thermostat and <topic>_boiler, alternating with the Write-Data value -> visible 100->0->100 flap in HA when bSeparateSources is enabled. Fix restores the documented ADR-066 intent; no new decision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Predicate at MQTTstuff.ino:1212 is replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
- [ ] #2 Comment block immediately above the gate is updated so a future reader can see why OTdata.type (not rsptype) is the right field
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
- [ ] #5 Prerelease bump committed alongside the firmware change via bin/bump-prerelease.sh
- [ ] #6 Field-validation note in Final Summary: with bSeparateSources=true, msgid 14 and 16 on _thermostat/_boiler no longer flap to 0 between Write-Data frames (tester sign-off via Discord; leave blocking AC if not yet confirmed)
<!-- AC:END -->
