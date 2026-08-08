---
id: TASK-1068
title: >-
  feat-2.0.0: port TASK-1064 — decode Remeha MsgIDs 131-133 (enum sits 3 ids
  below OTmap)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 15:42'
updated_date: '2026-08-08 15:55'
labels:
  - bug
  - opentherm
dependencies: []
priority: medium
ordinal: 255000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x TASK-1064, verified present on this branch by computing the enum values from source: OTLibMessageID (src/OTGW-firmware/OTGW-Core.h:228, 116 members) places OT_RemehadFdUcodes at 128, OT_RemehaServicemessage at 129 and OT_RemehaDetectionConnectedSCU at 130, while OTmap[] declares them at 131/132/133 (:494 onwards) with empty OT_UNDEF placeholders at 128-130. OT_MasterVersion=126 and OT_SlaveVersion=127 both align correctly, so the offset is specific to these three. processOT casts the id straight to the enum, so the tables must agree: MsgIDs 131-133 decode as 'Unknown message' and produce no labelled topic, while 128-130 emit label-less output. Authoritative numbering is in docs/opentherm specification/New OT data-ids.txt. Fix mirrors 1.x commit ef12138cf: an explicit = 131 on the first Remeha member, following the idiom the enum already uses to bridge id gaps.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The three Remeha enum members are renumbered to 131/132/133 to match OTmap and the spec file
- [x] #2 Exactly three enum members change value, verified by diffing computed enum values before and after; none added or removed
- [x] #3 Build green for the relevant esp32 targets, verified on artifact freshness and the per-env SUCCESS line
- [x] #4 python evaluate.py --quick shows no new failures
- [x] #5 Behaviour matches the otgw-1.x.x implementation
- [ ] #6 On-device verification that ids 131-133 decode to their labels (blocked: needs ESP32 hardware)
<!-- AC:END -->
