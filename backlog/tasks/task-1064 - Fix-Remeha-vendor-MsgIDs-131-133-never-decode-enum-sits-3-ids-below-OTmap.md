---
id: TASK-1064
title: 'Fix: Remeha vendor MsgIDs 131-133 never decode (enum sits 3 ids below OTmap)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 10:52'
updated_date: '2026-08-08 13:28'
labels:
  - bug
  - opentherm
dependencies: []
priority: medium
ordinal: 174000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1063 baseline validation run on v1.7.3-beta.3. OpenThermMessageID in OTGW-Core.h is id-indexed (7 explicit assignments keep it aligned; OT_MasterVersion=126 and OT_SlaveVersion=127 both match OTmap). The three Remeha vendor entries break that alignment: OT_RemehadFdUcodes=128, OT_RemehaServicemessage=129, OT_RemehaDetectionConnectedSCU=130, while OTmap[] declares them at 131, 132 and 133 and puts empty OT_UNDEF placeholders at 128-130. processOT casts OTdata.id straight to OpenThermMessageID and dispatches on it, so the two tables must agree. Authoritative numbering is in the repo: docs/opentherm specification/New OT data-ids.txt lists ID 131 dF-/dU-codes, ID 132 Servicemessage, ID 133 detection connected SCUs. OTmap is therefore correct and the enum is wrong. Observed on device: ids 131/132/133 decode as 'Unknown message [131] value [830305]' instead of their mapped labels, so a real Remeha boiler sending them gets no decode and no labelled MQTT topic; ids 128/129/130 instead hit the Remeha decode cases and emit label-less lines ('= 0 / 0') because OTmap[128..130] carry empty labels. Pre-existing, not a beta.3 regression.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OT_RemehadFdUcodes, OT_RemehaServicemessage and OT_RemehaDetectionConnectedSCU are renumbered to 131, 132 and 133 to match OTmap and the spec file
- [x] #2 MsgIDs 131/132/133 decode to their mapped labels instead of 'Unknown message'
- [x] #3 MsgIDs 128/129/130 no longer hit the Remeha decode cases and stop emitting label-less output
- [x] #4 No other enum member changes numeric value (verify by diffing computed enum values before and after)
- [x] #5 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures
- [ ] #6 Coverage baseline is re-recorded deliberately, with the diff reviewed and shown to contain only the intended id 128-133 changes
<!-- AC:END -->
