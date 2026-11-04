---
id: TASK-1068
title: >-
  feat-2.0.0: port TASK-1064 — decode Remeha MsgIDs 131-133 (enum sits 3 ids
  below OTmap)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-08 15:42'
updated_date: '2026-10-03 09:39'
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
- [x] #6 On-device verification that ids 131-133 decode to their labels (blocked: needs ESP32 hardware)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-25 backlog sweep: code is implemented and committed on this branch; verified by git rather than by the task file. TASK-1068 and TASK-1069 both landed in a7e06f8df; TASK-1052's shim is at platform_esp32.h:234 with the call at networkStuff.ino:92. Every AC except the on-device one is met.

Left In Progress deliberately. The remaining AC needs ESP32 hardware in the loop, which no amount of code reading can substitute for, and flipping the task to Done would claim a verification that never happened.

AC#6 ON-DEVICE, 2026-10-03, OTGW32 192.168.88.61 running 2.0.0-alpha.404+1d9ae71 (contains the enum fix 9bfcd0868: OT_RemehadFdUcodes = 131, OTGW-Core.h:353).
Method: the /otgw_simulation.log frame replay (TASK-1071, OT-Direct). The original fixture was backed up (sha256 6f88a78ea743fd57, 12716 B, identical to src/OTGW-firmware/data/otgw_simulation.log). A 30-line fixture with parity-correct T READ / B READ_ACK pairs replaced it: 131 data 0x1234, 132 0x5678, 133 0x0A0B (lines T80830000/BC0831234, T00840000/BC0845678, T80850000/BC0850A0B, five times). Then POST /api/v2/simulate/start (interval 750 ms), 20 s wait, reads, stop.
- Before: GET /api/v2/otgw/messages/131|132|133 gave RemehadFdUcodes 0, RemehaServicemessage 0, RemehaDetectionConnectedSCU 0.
- After: 131 RemehadFdUcodes 4660 (=0x1234), 132 RemehaServicemessage 22136 (=0x5678), 133 RemehaDetectionConnectedSCU 2571 (=0x0A0B). GET /api/v2/otgw/label/<label> gives the same three values.
Each id decodes under its own label. With the old enum (128-130), getOTGWValue's case OT_RemehadFdUcodes matches 128, so 131 would have stayed 0.
The original fixture was restored and read back (sha256 6f88a78ea743fd57, 12716 B).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported 1.x TASK-1064. The three Remeha members of OTLibMessageID (OT_RemehadFdUcodes, OT_RemehaServicemessage, OT_RemehaDetectionConnectedSCU) sat at 128-130, while OTmap and the OT spec file put them at 131-133. They are renumbered to 131/132/133 (commit 9bfcd0868).
Evidence:
- AC#1-#5: computed enum values diffed before and after, exactly three members changed; build and evaluate green at the time; behaviour matches otgw-1.x.x.
- AC#6 (2026-10-03, on the OTGW32 bench, alpha.404): parity-correct READ_ACK frames for 131/132/133 (data 0x1234, 0x5678, 0x0A0B) were replayed through /otgw_simulation.log. GET /api/v2/otgw/messages/<id> and /label/<label> went from 0 to 4660, 22136 and 2571, each under its own label. The original fixture was restored (sha256 verified).
<!-- SECTION:FINAL_SUMMARY:END -->
