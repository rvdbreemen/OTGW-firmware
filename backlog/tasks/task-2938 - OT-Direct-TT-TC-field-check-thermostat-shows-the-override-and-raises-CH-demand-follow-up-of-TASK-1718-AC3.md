---
id: TASK-2938
title: >-
  OT-Direct TT=/TC= field check: thermostat shows the override and raises CH
  demand (follow-up of TASK-1718 AC3)
status: To Do
assignee: []
created_date: '2026-11-20T01:17:00Z'
labels:
  - otdirect
  - field
milestone: 2.1.0
dependencies: []
ordinal: 1069000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field part of TASK-1718 AC3, waived for 2.0.0 by Robert (2026-11-09, relayed by the coordinator; recorded in commit 8f1e3f4b4 and as a known issue in docs/releases/RELEASE_NOTES_2.0.0-draft.md:351). The fix (beta.142) answers the thermostat's MsgID 9 READ with the TT=/TC= value. tjfs's beta.161 capture (OTGW-field-data/tjfs/2026-11-09_2.0.0-beta.161+45dadcf, TT=21.5 at 10:09:47, TT=0 at 10:10:40) has no thermostat MsgID 9 READ at all: the only MsgID 9 frame is the gateway's own R 00090000 -> B C0090000 at 09:58. So that thermostat never asks for the override and cannot show it. Needs a thermostat that reads MsgID 9 in OT-Direct gateway mode.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A field or bench capture in OT-Direct gateway mode with TT= active shows the thermostat's MsgID 9 READ answered with the override value
- [ ] #2 The same capture shows the thermostat taking over the setpoint and raising CH demand (MsgID 0 master status bit 0)
<!-- AC:END -->
