---
id: TASK-1069
title: >-
  feat-2.0.0: port TASK-1066 — four vh_* status bits never republish (StatusVH
  slot collision)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 15:43'
updated_date: '2026-08-08 15:55'
labels:
  - bug
  - mqtt
dependencies: []
priority: medium
ordinal: 256000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x TASK-1066, verified present on this branch: publishStatusVHBitMQTT is called with slots 0,1,2,3 for the master (HB) bits AND slots 0,1,2,3,4,6 for the slave (LB) bits, so slots 0-3 are used twice. mqttlastsentstatusvhbit[] documents its own contract as 'slots 0-7=master, 8-15=slave' and the OT_Statusflags fan-out honours that, using 8-15 for slave bits. The master fan-out stamps slots 0-3 microseconds before the slave fan-out reads them, so elapsedTrackedSeconds is ~0 and the 60s heartbeat never elapses. Consequence: vh_fault, vh_ventilation_mode, vh_bypass_status and vh_bypass_automatic_status publish only on first-seen or on a force, never on their interval. vh_free_ventliation_status (slot 4) and vh_diagnostic_indicator (slot 6) are unaffected because they have no master counterpart, which is the signature that identifies the bug. Fix mirrors 1.x commit ef12138cf: slave bits move to 8,9,10,11,12,14.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 StatusVH slave bits use slots 8-14, matching the declared contract and the OT_Statusflags fan-out
- [x] #2 No duplicate slot index remains across the master and slave StatusVH fan-outs
- [x] #3 Build green for the relevant esp32 targets, verified on artifact freshness and the per-env SUCCESS line
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 Behaviour matches the otgw-1.x.x implementation
- [ ] #6 On-device verification that all four topics republish on their heartbeat (blocked: needs ESP32 hardware)
<!-- AC:END -->
