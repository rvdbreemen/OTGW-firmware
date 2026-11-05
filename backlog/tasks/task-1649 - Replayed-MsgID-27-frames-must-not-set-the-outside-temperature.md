---
id: TASK-1649
title: Replayed MsgID 27 frames must not set the outside temperature
status: To Do
assignee: []
created_date: '2026-11-05T06:57:00Z'
updated_date: '2026-11-05T07:13:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 773000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://discord.com none; found by TASK-1412.20 (R-3): OTGW-Core.ino:6076-6078 stamps the outside temperature from a replayed MsgID 27 frame (no bReplayed check), while docs/api/MQTT.md says live Read-Ack only. Robert 2026-11-05: bug task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Replayed MsgID 27 leaves the outside temperature unchanged; live Read-Ack still sets it
- [ ] #2 Host test red on old / green on fix calling the real code path; evaluate --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05: fix (sonnet agent, wt-1649): the TASK-1390 MsgID 27 stamp block in OTGW-Core.ino ~6075 also requires !OTdata.bReplayed (same flag as the MsgID 25/28 block). New host test test/host/test_replay_frame_guards.py (slices the real block): old 2 FAIL of 5, fix 5/5 PASS. Robert 2026-11-05 (session): only the stamp; the decoded value Toutside is still written by print_f88 for replayed frames (replay is published like a bus frame by design, OTGW-Core.h:830); residual risk accepted: during a replay a replayed value can sit under an older live stamp. AC1 reworded in effect: the stamp/age is unchanged by a replayed frame.
<!-- SECTION:NOTES:END -->
